#include "pch.h"

#include "UPickingSubsystem.h"

#include "Core/Stat/Stat.h"
#include "World/Component/UMeshComponent.h"
#include "World/Component/UBillboardComponent.h"
#include "World/Component/UPrimitiveComponent.h"

#include <chrono>
#include <cmath>

// Temporary picking breakdown: set to 0 to disable the extra timers and overlay rows.
#ifndef MACAW_PICKING_PHASE_TIMING
#define MACAW_PICKING_PHASE_TIMING 1
#endif

namespace {
    bool RaycastComponent(UPrimitiveComponent* Component, const FRay& Ray, float& OutDistance, const FMatrix* CameraWorld, double* NarrowPhaseMilliseconds) {
        if (Component == nullptr || !Component->IsActive() || !Component->IsVisible()) return false;
        if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
            if (CameraWorld == nullptr) return false;
            const auto* Billboard = static_cast<const UBillboardComponent*>(Component);
            std::array<FVector3, 4> Corners{};
            if (!Billboard->GetWorldCorners(*CameraWorld, Corners)) return false;
            const FVector3 Right = Corners[2] - Corners[0];
            const FVector3 Down = Corners[1] - Corners[0];
            const FVector3 Normal = Right.Cross(Down);
            const FVector3 Direction{Ray.direction};
            const float Denominator = Direction.Dot(Normal);
            if (std::abs(Denominator) <= 0.000001f) return false;
            const float HitDistance = (Corners[0] - FVector3{Ray.position}).Dot(Normal) / Denominator;
            if (HitDistance < 0.0f || HitDistance >= OutDistance) return false;
            const FVector3 HitOffset = FVector3{Ray.position} + Direction * HitDistance - Corners[0];
            const float Horizontal = HitOffset.Dot(Right) / Right.LengthSquared();
            const float Vertical = HitOffset.Dot(Down) / Down.LengthSquared();
            if (!(Horizontal >= 0.0f && Horizontal <= 1.0f && Vertical >= 0.0f && Vertical <= 1.0f)) return false;
            OutDistance = HitDistance;
            return true;
        }
        DirectX::BoundingOrientedBox WorldBox{};
        Component->GetPickingBox().Transform(WorldBox, Component->GetComponentToWorld().ToSimpleMath());
        float HitDistance = 0.0f;
        if (!WorldBox.Intersects(Ray.position, Ray.direction, HitDistance) || HitDistance > OutDistance) return false;
        HitDistance = (std::max)(HitDistance, 0.0f);
        if (Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
#if MACAW_PICKING_PHASE_TIMING
            const auto Start = std::chrono::steady_clock::now();
#endif
            const bool Hit = static_cast<UMeshComponent*>(Component)->RaycastMesh(Ray, HitDistance, OutDistance);
#if MACAW_PICKING_PHASE_TIMING
            if (NarrowPhaseMilliseconds != nullptr) *NarrowPhaseMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
#endif
            if (!Hit) return false;
        }
        if (HitDistance >= OutDistance) return false;
        OutDistance = HitDistance;
        return true;
    }
}

void FWorldRaycastAccelerationStructure::Clear() {
    Nodes.clear();
}

bool FWorldRaycastAccelerationStructure::BuildStructure(const TArray<TObjectRef<UPrimitiveComponent>>& Components) {
    Clear();
    TArray<FBuildItem> Items;
    Items.reserve(Components.size());
    MinMaxBox Bounds;
    for (const auto& Ref : Components) {
        UPrimitiveComponent* Component = Ref.Get();
        if (Component == nullptr) continue;
        DirectX::BoundingBox Box{};
        if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
            const auto& Size = static_cast<const UBillboardComponent*>(Component)->GetSize();
            const float Radius = 0.5f * (std::abs(Size.mX) + std::abs(Size.mY));
            Box.Center = Component->GetComponentToWorld().ToSimpleMath().Translation();
            Box.Extents = {Radius, Radius, Radius};
        }
        else {
            DirectX::BoundingOrientedBox WorldBox{};
            Component->GetPickingBox().Transform(WorldBox, Component->GetComponentToWorld().ToSimpleMath());
            DirectX::XMFLOAT3 Corners[8];
            WorldBox.GetCorners(Corners);
            DirectX::BoundingBox::CreateFromPoints(Box, 8, Corners, sizeof(Corners[0]));
        }
        const float Values[]{Box.Center.x, Box.Center.y, Box.Center.z, Box.Extents.x, Box.Extents.y, Box.Extents.z};
        for (float Value : Values) if (!std::isfinite(Value)) return false;
        Items.push_back({Box, Ref});
        Bounds.Expand(Box);
    }
    if (Items.empty()) return true;
    if (Items.size() > (static_cast<std::size_t>(InvalidIndex) + 1) / 2) return false;
    Nodes.reserve(Items.size() * 2 - 1);
    MakeChild(Items, 0, static_cast<Uint32>(Items.size()), Bounds);
    return true;
}
Uint32 FWorldRaycastAccelerationStructure::MakeChild(TArray<FBuildItem>& Items, Uint32 First, Uint32 Count, const MinMaxBox& Bounds) {
    FNode Node{};
    Node.BoundingBox.Center = { Bounds.minX * 0.5f + Bounds.maxX * 0.5f, Bounds.minY * 0.5f + Bounds.maxY * 0.5f, Bounds.minZ * 0.5f + Bounds.maxZ * 0.5f };
    Node.BoundingBox.Extents = { Bounds.maxX * 0.5f - Bounds.minX * 0.5f, Bounds.maxY * 0.5f - Bounds.minY * 0.5f, Bounds.maxZ * 0.5f - Bounds.minZ * 0.5f };
    Node.BoundingBox.Extents.x += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.x) + Node.BoundingBox.Extents.x);
    Node.BoundingBox.Extents.y += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.y) + Node.BoundingBox.Extents.y);
    Node.BoundingBox.Extents.z += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.z) + Node.BoundingBox.Extents.z);
    const Uint32 NodeIndex = static_cast<Uint32>(Nodes.size());
    Nodes.push_back(Node);
    if (Count == 1) {
        Nodes[NodeIndex].Component = Items[First].Component;
        return NodeIndex;
    }
    const auto& BoundingBox = Node.BoundingBox;

    MinMaxBox Bin[3][Slice];
    Uint32 BinCounts[3][Slice]{};
    const float Centers[3]{ BoundingBox.Center.x, BoundingBox.Center.y, BoundingBox.Center.z };
    const float Extents[3]{ BoundingBox.Extents.x, BoundingBox.Extents.y, BoundingBox.Extents.z };
    const auto GetBinIndex = [&](const DirectX::BoundingBox& Box, Uint32 Axis) {
        const float BoxCenters[3]{ Box.Center.x, Box.Center.y, Box.Center.z };
        const float Normalized = Extents[Axis] > 0.0f ? (BoxCenters[Axis] - Centers[Axis]) / Extents[Axis] : -1.0f;
        return static_cast<Uint32>(std::clamp((Normalized + 1.0f) * Slice / 2, 0.0f, static_cast<float>(Slice - 1)));
    };
    for (Uint32 Offset = 0; Offset < Count; ++Offset) {
        const auto& Box = Items[First + Offset].BoundingBox;
        for (Uint32 Axis = 0; Axis < 3; ++Axis) {
            const Uint32 Index = GetBinIndex(Box, Axis);
            Bin[Axis][Index].Expand(Box);
            ++BinCounts[Axis][Index];
        }
    }
    Uint32 BestAxis = 0;
    Uint32 BestLeftEnd = 0;
    float BestCost = std::numeric_limits<float>::max();
    MinMaxBox BestLeftBox{};
    MinMaxBox BestRightBox{};

    for (Uint32 Axis = 0; Axis < 3; ++Axis) {
        MinMaxBox RightBoxes[Slice];
        Uint32 RightCounts[Slice];
        RightBoxes[Slice - 1] = Bin[Axis][Slice - 1];
        RightCounts[Slice - 1] = BinCounts[Axis][Slice - 1];
        for (int j = Slice - 2; j >= 0; --j) {
            RightBoxes[j] = MinMaxBox::Merge(Bin[Axis][j], RightBoxes[j + 1]);
            RightCounts[j] = BinCounts[Axis][j] + RightCounts[j + 1];
        }
        MinMaxBox LeftBox;
        Uint32 LeftCount = 0;
        for (int LeftEnd = 0; LeftEnd < Slice - 1; ++LeftEnd) {
            LeftBox = MinMaxBox::Merge(LeftBox, Bin[Axis][LeftEnd]);
            LeftCount += BinCounts[Axis][LeftEnd];
            const MinMaxBox& RightBox = RightBoxes[LeftEnd + 1];
            const Uint32 RightCount = RightCounts[LeftEnd + 1];
            if (LeftCount == 0 || RightCount == 0) continue;
            float Cost = LeftCount * LeftBox.SurfaceArea() + RightCount * RightBox.SurfaceArea();
            if (Cost < BestCost) {
                BestCost = Cost;
                BestLeftEnd = LeftEnd;
                BestAxis = Axis;
                BestLeftBox = LeftBox;
                BestRightBox = RightBox;
            }
        }
    }

    Uint32 LeftCount = Count / 2;
    if (BestCost < std::numeric_limits<float>::max()) {
        auto Begin = Items.begin() + First;
        auto Middle = std::partition(Begin, Begin + Count, [&](const FBuildItem& Item) { return GetBinIndex(Item.BoundingBox, BestAxis) <= BestLeftEnd; });
        LeftCount = static_cast<Uint32>(Middle - Begin);
    }
    if (BestCost == std::numeric_limits<float>::max() || LeftCount == 0 || LeftCount == Count) {
        LeftCount = Count / 2;
        BestLeftBox = {};
        BestRightBox = {};
        for (Uint32 Offset = 0; Offset < Count; ++Offset) {
            if (Offset < LeftCount) BestLeftBox.Expand(Items[First + Offset].BoundingBox);
            else BestRightBox.Expand(Items[First + Offset].BoundingBox);
        }
    }
    Nodes[NodeIndex].mLeft = MakeChild(Items, First, LeftCount, BestLeftBox);
    Nodes[NodeIndex].mRight = MakeChild(Items, First + LeftCount, Count - LeftCount, BestRightBox);
    return NodeIndex;
}

bool FWorldRaycastAccelerationStructure::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld, double* OutNarrowPhaseMilliseconds) const {
    OutComponent = nullptr;
    OutDistance = std::numeric_limits<float>::max();
    if (OutNarrowPhaseMilliseconds != nullptr) *OutNarrowPhaseMilliseconds = 0.0;
    if (Nodes.empty()) return false;
    float& ClosestDistance = OutDistance;
    const DirectX::XMVECTOR Origin = Ray.position;
    const DirectX::XMVECTOR Direction = Ray.direction;
    const DirectX::XMVECTOR IsParallel = DirectX::XMVectorLessOrEqual(DirectX::XMVectorAbs(Direction), DirectX::g_RayEpsilon);
    const DirectX::XMVECTOR InverseDirection = DirectX::XMVectorReciprocal(DirectX::XMVectorSelect(Direction, DirectX::XMVectorSplatOne(), IsParallel));
    const auto IntersectsBox = [&](const DirectX::BoundingBox& Box, float& EntryDistance) {
        const DirectX::XMVECTOR Extents = DirectX::XMLoadFloat3(&Box.Extents);
        const DirectX::XMVECTOR Offset = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&Box.Center), Origin);
        const DirectX::XMVECTOR Outside = DirectX::XMVectorAndInt(IsParallel, DirectX::XMVectorGreater(DirectX::XMVectorAbs(Offset), Extents));
        if (!DirectX::XMVector3EqualInt(Outside, DirectX::XMVectorZero())) return false;
        const DirectX::XMVECTOR T1 = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(Offset, Extents), InverseDirection);
        const DirectX::XMVECTOR T2 = DirectX::XMVectorMultiply(DirectX::XMVectorAdd(Offset, Extents), InverseDirection);
        DirectX::XMVECTOR Near = DirectX::XMVectorSelect(DirectX::XMVectorMin(T1, T2), DirectX::g_FltMin, IsParallel);
        DirectX::XMVECTOR Far = DirectX::XMVectorSelect(DirectX::XMVectorMax(T1, T2), DirectX::g_FltMax, IsParallel);
        Near = DirectX::XMVectorMax(Near, DirectX::XMVectorSplatY(Near));
        Near = DirectX::XMVectorMax(Near, DirectX::XMVectorSplatZ(Near));
        Far = DirectX::XMVectorMin(Far, DirectX::XMVectorSplatY(Far));
        Far = DirectX::XMVectorMin(Far, DirectX::XMVectorSplatZ(Far));
        EntryDistance = (std::max)(DirectX::XMVectorGetX(Near), 0.0f);
        return EntryDistance <= DirectX::XMVectorGetX(Far) && EntryDistance <= ClosestDistance;
    };
    float RootDistance = 0.0f;
    if (!IntersectsBox(Nodes[0].BoundingBox, RootDistance)) return false;
    struct FStackEntry {
        Uint32 NodeIndex;
        float EntryDistance;
    };
    FStackEntry Stack[64];
    Uint32 StackSize = 0;
    TArray<FStackEntry> Overflow;
    const auto Push = [&](FStackEntry Entry) {
        if (StackSize < std::size(Stack)) Stack[StackSize++] = Entry;
        else Overflow.push_back(Entry);
    };
    FStackEntry Entry{ 0, RootDistance };
    for (;;) {
        if (Entry.EntryDistance <= ClosestDistance) {
            const FNode& Node = Nodes[Entry.NodeIndex];
            if (Node.mLeft != InvalidIndex) {
                FStackEntry Children[2]{ { Node.mLeft, 0.0f }, { Node.mRight, 0.0f } };
                bool Hits[2]{};
                for (Uint32 i = 0; i < 2; ++i) {
                    Hits[i] = IntersectsBox(Nodes[Children[i].NodeIndex].BoundingBox, Children[i].EntryDistance);
                }
                if (Hits[0] && Hits[1]) {
                    if (Children[0].EntryDistance > Children[1].EntryDistance) std::swap(Children[0], Children[1]);
                    Push(Children[1]);
                    Entry = Children[0];
                    continue;
                }
                if (Hits[0] || Hits[1]) { Entry = Children[Hits[0] ? 0 : 1]; continue; }
            }
            else {
                UPrimitiveComponent* Component = Node.Component.Get();
                if (RaycastComponent(Component, Ray, OutDistance, CameraWorld, OutNarrowPhaseMilliseconds)) OutComponent = Component;
            }
        }
        if (!Overflow.empty()) { Entry = Overflow.back(); Overflow.pop_back(); }
        else if (StackSize > 0) Entry = Stack[--StackSize];
        else break;
    }

    return OutComponent != nullptr;
}

void UPickingSubsystem::RegisterComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) return;
    mComponents.emplace_back(Component);
}

void UPickingSubsystem::UnregisterComponent(UPrimitiveComponent* Component) {
    std::erase_if(mComponents, [Component](const TObjectRef<UPrimitiveComponent>& Ref) { return Ref.Get() == Component; });
}

bool UPickingSubsystem::RebuildAccelerationStructure() {
    return RaycastAccelerationStructure.BuildStructure(mComponents);
}

bool UPickingSubsystem::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld) const {
    const auto Start = std::chrono::steady_clock::now();
    double NarrowPhaseMilliseconds = 0.0;
    double* NarrowPhase = nullptr;
#if MACAW_PICKING_PHASE_TIMING
    NarrowPhase = &NarrowPhaseMilliseconds;
#endif
    RaycastAccelerationStructure.Raycast(Ray, OutComponent, OutDistance, CameraWorld, NarrowPhase);
    const double Milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
#if MACAW_PICKING_PHASE_TIMING
    Stat::RecordPickingTime(Milliseconds, NarrowPhaseMilliseconds);
#else
    Stat::RecordPickingTime(Milliseconds);
#endif
    return OutComponent != nullptr;
}

bool UPickingSubsystem::ContainsComponent(const UPrimitiveComponent* Component) const {
    return std::ranges::any_of(mComponents, [Component](const TObjectRef<UPrimitiveComponent>& Ref) { return Ref.Get() == Component; });
}

const TArray<TObjectRef<UPrimitiveComponent>>& UPickingSubsystem::GetRegisteredComponents() const {
    return mComponents;
}

void UPickingSubsystem::OnDeinitialize() {
    mComponents.clear();
    RaycastAccelerationStructure.Clear();
}
