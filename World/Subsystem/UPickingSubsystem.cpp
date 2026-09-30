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

FWorldRaycastAccelerationStructure::FProxy FWorldRaycastAccelerationStructure::MakeProxy(UPrimitiveComponent* Component) {
    FProxy Proxy{};
    Proxy.Component.Set(Component);
    Proxy.Enabled = Component->IsActive() && Component->IsVisible();
    if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
        Proxy.Kind = EProxyKind::Billboard;
        const auto* Billboard = static_cast<const UBillboardComponent*>(Component);
        Proxy.BillboardSize = Billboard->GetSize();
        FBillboardProbe Probe{};
        const bool Renderable = Billboard->MakeBillboardRender(Probe);
        Proxy.Enabled = Proxy.Enabled && Renderable && Proxy.BillboardSize.mX > 0.0f && Proxy.BillboardSize.mY > 0.0f;
        Proxy.Box.Center = Renderable ? Probe.mWorld.Translation().ToSimpleMath() : Component->GetComponentLocation().ToSimpleMath();
    } else {
        Proxy.Box = Component->GetWorldOBB();
        if (Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
            Proxy.Kind = EProxyKind::Mesh;
            Proxy.Mesh.Update(static_cast<const UMeshComponent*>(Component)->ResolveMesh(), Component->GetComponentTransform());
        }
    }
    return Proxy;
}

bool FWorldRaycastAccelerationStructure::GetProxyBounds(const FProxy& Proxy, DirectX::BoundingBox& Box) {
    if (Proxy.Kind == EProxyKind::Billboard) {
        const float Radius = 0.5f * (std::abs(Proxy.BillboardSize.mX) + std::abs(Proxy.BillboardSize.mY));
        Box = {Proxy.Box.Center, {Radius, Radius, Radius}};
    } else {
        DirectX::XMFLOAT3 Corners[8];
        Proxy.Box.GetCorners(Corners);
        DirectX::BoundingBox::CreateFromPoints(Box, 8, Corners, sizeof(Corners[0]));
    }
    const float Values[]{Box.Center.x, Box.Center.y, Box.Center.z, Box.Extents.x, Box.Extents.y, Box.Extents.z};
    for (float Value : Values) if (!std::isfinite(Value)) return false;
    return true;
}

bool FWorldRaycastAccelerationStructure::RaycastProxy(const FProxy& Proxy, const FRay& Ray, float& OutDistance, const FMatrix* CameraWorld, double* NarrowPhaseMilliseconds) {
    if (!Proxy.Enabled) return false;
    if (Proxy.Kind == EProxyKind::Billboard) {
        if (CameraWorld == nullptr) return false;
        FVector3 Right{CameraWorld->m_[0][0], CameraWorld->m_[0][1], CameraWorld->m_[0][2]};
        FVector3 Up{CameraWorld->m_[1][0], CameraWorld->m_[1][1], CameraWorld->m_[1][2]};
        if (Right.LengthSquared() <= 0.0f || Up.LengthSquared() <= 0.0f) return false;
        Right.Normalize(); Up.Normalize();
        const FVector3 Corner = FVector3{Proxy.Box.Center} - Right * (Proxy.BillboardSize.mX * 0.5f) + Up * (Proxy.BillboardSize.mY * 0.5f);
        Right *= Proxy.BillboardSize.mX;
        const FVector3 Down = Up * -Proxy.BillboardSize.mY;
        const FVector3 Normal = Right.Cross(Down), Direction{Ray.direction};
        const float Denominator = Direction.Dot(Normal);
        if (std::abs(Denominator) <= 0.000001f) return false;
        const float HitDistance = (Corner - FVector3{Ray.position}).Dot(Normal) / Denominator;
        if (HitDistance < 0.0f || HitDistance >= OutDistance) return false;
        const FVector3 HitOffset = FVector3{Ray.position} + Direction * HitDistance - Corner;
        const float Horizontal = HitOffset.Dot(Right) / Right.LengthSquared(), Vertical = HitOffset.Dot(Down) / Down.LengthSquared();
        if (!(Horizontal >= 0.0f && Horizontal <= 1.0f && Vertical >= 0.0f && Vertical <= 1.0f)) return false;
        OutDistance = HitDistance;
        return true;
    }
    float HitDistance = 0.0f;
    if (!Proxy.Box.Intersects(Ray.position, Ray.direction, HitDistance) || HitDistance > OutDistance) return false;
    HitDistance = (std::max)(HitDistance, 0.0f);
    if (Proxy.Kind == EProxyKind::Mesh) {
#if MACAW_PICKING_PHASE_TIMING
        const auto Start = std::chrono::steady_clock::now();
#endif
        const bool Hit = Proxy.Mesh.Raycast(Ray, HitDistance, OutDistance);
#if MACAW_PICKING_PHASE_TIMING
        if (NarrowPhaseMilliseconds != nullptr) *NarrowPhaseMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
#endif
        if (!Hit) return false;
    }
    if (HitDistance >= OutDistance) return false;
    OutDistance = HitDistance;
    return true;
}

void FWorldRaycastAccelerationStructure::Clear() {
    mTree.Clear();
    mLeaves.clear();
    mLeafParents.clear();
    mNodeParents.clear();
    mLeafIndices.clear();
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
        if (!GetProxyBounds(MakeProxy(Component), Box)) { Clear(); return false; }
        Items.push_back({Box, Ref});
        Bounds.Expand(Box);
    }
    if (Items.empty()) return true;
    if (Items.size() > (static_cast<std::size_t>(InvalidIndex) + 1) / 2) return false;
    TArray<FNode> BuildNodes;
    BuildNodes.reserve(Items.size() * 2 - 1);
    MakeChild(BuildNodes, Items, 0, static_cast<Uint32>(Items.size()), Bounds);
    mLeaves.reserve(Items.size());
    mTree.Build(BuildNodes, [](const FNode& Node) { return Node.mLeft == InvalidIndex; }, [&](const FNode& Node) {
        const Uint32 Index = static_cast<Uint32>(mLeaves.size());
        mLeaves.push_back(MakeProxy(Node.Component.Get()));
        mLeafIndices.emplace(ComponentKey(Node.Component.GetHandle()), Index);
        return Index;
    });
    mLeafParents.resize(mLeaves.size());
    mNodeParents.resize(mTree.GetNodes().size());
    const auto MapParents = [&](auto&& Self, Uint32 Reference) -> void {
        const auto& Node = mTree.GetNodes()[Reference & BVH8::IndexMask];
        for (Uint32 Lane = 0; Lane < BVH8::GetCount(Reference); ++Lane) {
            const Uint32 Child = Node.Children[Lane], Index = Child & BVH8::IndexMask;
            if (Child & BVH8::LeafBit) mLeafParents[Index] = {Reference, Lane};
            else { mNodeParents[Index] = {Reference, Lane}; Self(Self, Child); }
        }
    };
    MapParents(MapParents, mTree.GetRootReference());
    return true;
}

void FWorldRaycastAccelerationStructure::UpdateComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr) return;
    const auto It = mLeafIndices.find(ComponentKey(Component->GetHandle()));
    if (It == mLeafIndices.end()) return;
    auto& Proxy = mLeaves[It->second];
    DirectX::BoundingBox PreviousBox;
    const bool HadBounds = GetProxyBounds(Proxy, PreviousBox);
    Proxy = MakeProxy(Component);
    DirectX::BoundingBox Box;
    if (!GetProxyBounds(Proxy, Box)) { Proxy.Enabled = false; return; }
    if (HadBounds && DirectX::XMVector3Equal(DirectX::XMLoadFloat3(&Box.Center), DirectX::XMLoadFloat3(&PreviousBox.Center)) && DirectX::XMVector3Equal(DirectX::XMLoadFloat3(&Box.Extents), DirectX::XMLoadFloat3(&PreviousBox.Extents))) return;
    FParent Parent = mLeafParents[It->second];
    while (Parent.Reference != InvalidIndex) {
        Box = mTree.RefitChild(Parent.Reference, Parent.Lane, Box);
        Parent = mNodeParents[Parent.Reference & BVH8::IndexMask];
    }
}

void FWorldRaycastAccelerationStructure::RemoveComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr) return;
    const auto It = mLeafIndices.find(ComponentKey(Component->GetHandle()));
    if (It == mLeafIndices.end()) return;
    mLeaves[It->second].Enabled = false;
    mLeafIndices.erase(It);
}

Uint32 FWorldRaycastAccelerationStructure::MakeChild(TArray<FNode>& BuildNodes, TArray<FBuildItem>& Items, Uint32 First, Uint32 Count, const MinMaxBox& Bounds) {
    FNode Node{};
    Node.BoundingBox.Center = { Bounds.minX * 0.5f + Bounds.maxX * 0.5f, Bounds.minY * 0.5f + Bounds.maxY * 0.5f, Bounds.minZ * 0.5f + Bounds.maxZ * 0.5f };
    Node.BoundingBox.Extents = { Bounds.maxX * 0.5f - Bounds.minX * 0.5f, Bounds.maxY * 0.5f - Bounds.minY * 0.5f, Bounds.maxZ * 0.5f - Bounds.minZ * 0.5f };
    Node.BoundingBox.Extents.x += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.x) + Node.BoundingBox.Extents.x);
    Node.BoundingBox.Extents.y += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.y) + Node.BoundingBox.Extents.y);
    Node.BoundingBox.Extents.z += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.z) + Node.BoundingBox.Extents.z);
    const Uint32 NodeIndex = static_cast<Uint32>(BuildNodes.size());
    BuildNodes.push_back(Node);
    if (Count == 1) {
        BuildNodes[NodeIndex].Component = Items[First].Component;
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
    BuildNodes[NodeIndex].mLeft = MakeChild(BuildNodes, Items, First, LeftCount, BestLeftBox);
    BuildNodes[NodeIndex].mRight = MakeChild(BuildNodes, Items, First + LeftCount, Count - LeftCount, BestRightBox);
    return NodeIndex;
}

bool FWorldRaycastAccelerationStructure::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld, double* OutNarrowPhaseMilliseconds) const {
    OutComponent = nullptr;
    OutDistance = std::numeric_limits<float>::max();
    if (OutNarrowPhaseMilliseconds != nullptr) *OutNarrowPhaseMilliseconds = 0.0;
    TArray<Uint32> StaleLeaves;
    for (;;) {
        Uint32 Winner = InvalidIndex;
        OutDistance = std::numeric_limits<float>::max();
        mTree.Raycast(Ray, OutDistance, [&](Uint32 LeafIndex, float& ClosestDistance) {
            if (std::find(StaleLeaves.begin(), StaleLeaves.end(), LeafIndex) != StaleLeaves.end()) return false;
            if (!RaycastProxy(mLeaves[LeafIndex], Ray, ClosestDistance, CameraWorld, OutNarrowPhaseMilliseconds)) return false;
            Winner = LeafIndex;
            return true;
        });
        if (Winner == InvalidIndex) return false;
        OutComponent = mLeaves[Winner].Component.Get();
        if (OutComponent != nullptr) return true;
        StaleLeaves.push_back(Winner);
    }
}

void UPickingSubsystem::RegisterComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) return;
    mComponents.emplace_back(Component);
}

void UPickingSubsystem::UnregisterComponent(UPrimitiveComponent* Component) {
    RaycastAccelerationStructure.RemoveComponent(Component);
    std::erase_if(mComponents, [Component](const TObjectRef<UPrimitiveComponent>& Ref) { return Ref.Get() == Component; });
}

bool UPickingSubsystem::RebuildAccelerationStructure() {
    if (!RaycastAccelerationStructure.BuildStructure(mComponents)) return false;
    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();
    volatile double WarmupInput = 1.0;
    volatile double WarmupLength = std::hypot(WarmupInput, WarmupInput, WarmupInput);
    volatile float WarmupDistance = std::nextafter(static_cast<float>(WarmupLength), std::numeric_limits<float>::infinity());
    return true;
}

void UPickingSubsystem::UpdateComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr) return;
    const auto Handle = Component->GetHandle();
    const Uint64 Key = (static_cast<Uint64>(Handle.mGeneration) << 32) | Handle.mIndex;
    if (mDirtyComponentKeys.insert(Key).second) mDirtyComponents.emplace_back(Component);
}

void UPickingSubsystem::SynchronizeProxies() const {
    for (std::size_t i = 0; i < mDirtyComponents.size(); ++i) RaycastAccelerationStructure.UpdateComponent(mDirtyComponents[i].Get());
    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();
}

bool UPickingSubsystem::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld) const {
    const auto Start = std::chrono::steady_clock::now();
    SynchronizeProxies();
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
    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();
    RaycastAccelerationStructure.Clear();
}
