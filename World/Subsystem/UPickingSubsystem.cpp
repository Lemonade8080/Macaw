#include "pch.h"

#include "UPickingSubsystem.h"

#include "Core/Stat/Stat.h"
#include "Core/Spatial/FBVHBuildOptimization.h"
#include "World/Component/UMeshComponent.h"
#include "World/Component/UBillboardComponent.h"
#include "World/Component/UPrimitiveComponent.h"

#include <chrono>
#include <cmath>

namespace {
    bool AreBoundsFinite(const DirectX::BoundingBox& Box) {
        const float C[]{Box.Center.x, Box.Center.y, Box.Center.z}, E[]{Box.Extents.x, Box.Extents.y, Box.Extents.z};
        for (Uint32 i = 0; i < 3; ++i) if (!(E[i] >= 0.0f) || !std::isfinite(C[i] - E[i]) || !std::isfinite(C[i] + E[i])) return false;
        return true;
    }
}

FWorldRaycastAccelerationStructure::FProxy FWorldRaycastAccelerationStructure::MakeProxy(UPrimitiveComponent* Component, DirectX::BoundingBox& Bounds) {
    FProxy Proxy{};
    Proxy.Component.Set(Component);
    Proxy.Enabled = Component->IsActive() && Component->IsVisible();
    const Uint64 Key = ComponentKey(Component->GetHandle());
    if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
        Proxy.Kind = EProxyKind::Billboard;
        const auto* Billboard = static_cast<const UBillboardComponent*>(Component);
        Proxy.BillboardSize = Billboard->GetSize();
        FBillboardProbe Probe{};
        const bool Renderable = Billboard->MakeBillboardRender(Probe);
        Proxy.Enabled = Proxy.Enabled && Renderable && Proxy.BillboardSize.mX > 0.0f && Proxy.BillboardSize.mY > 0.0f;
        Proxy.Box.Center = Renderable ? Probe.mWorld.Translation().ToSimpleMath() : Component->GetComponentLocation().ToSimpleMath();
        const float Radius = 0.5f * (std::abs(Proxy.BillboardSize.mX) + std::abs(Proxy.BillboardSize.mY));
        Bounds = {Proxy.Box.Center, {Radius, Radius, Radius}};
    } else {
        DirectX::XMFLOAT3 Corners[8];
        if (Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
            Proxy.Kind = EProxyKind::Mesh;
            const auto& LocalBox = Component->GetPickingBox();
            const auto& Q = LocalBox.Orientation;
            Proxy.HasCustomBox = Q.x != 0.0f || Q.y != 0.0f || Q.z != 0.0f;
            if (Proxy.HasCustomBox) mLocalBoxes[Key] = LocalBox; else mLocalBoxes.erase(Key);
            LocalBox.GetCorners(Corners);
            DirectX::BoundingBox::CreateFromPoints(Proxy.Box, 8, Corners, sizeof(Corners[0]));
            const auto Matrix = Component->GetComponentToWorld().ToSimpleMath();
            for (auto& Corner : Corners) DirectX::XMStoreFloat3(&Corner, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&Corner), Matrix));
            Proxy.Mesh.Update(static_cast<const UMeshComponent*>(Component)->ResolveMesh(), Component->GetComponentTransform());
        } else {
            const auto& WorldBox = Component->GetWorldOBB();
            Proxy.Box = {{0, 0, 0}, WorldBox.Extents};
            Proxy.Mesh.Update(nullptr, FTransform{FVector3{WorldBox.Center}, FQuat{WorldBox.Orientation}, FVector3{1.0f}});
            WorldBox.GetCorners(Corners);
        }
        DirectX::BoundingBox::CreateFromPoints(Bounds, 8, Corners, sizeof(Corners[0]));
        const float Pad = 4.0f * std::numeric_limits<float>::epsilon();
        Bounds.Extents.x += Pad * (std::abs(Bounds.Center.x) + Bounds.Extents.x);
        Bounds.Extents.y += Pad * (std::abs(Bounds.Center.y) + Bounds.Extents.y);
        Bounds.Extents.z += Pad * (std::abs(Bounds.Center.z) + Bounds.Extents.z);
    }
    if (!AreBoundsFinite(Bounds)) Proxy.Enabled = false;
    return Proxy;
}

bool FWorldRaycastAccelerationStructure::RaycastProxy(const FProxy& Proxy, const FRay& Ray, float& OutDistance, const FMatrix* CameraWorld) const {
    if (!Proxy.Enabled) return false;
    if (Proxy.Mesh.Source != nullptr) _mm_prefetch(reinterpret_cast<const char*>(Proxy.Mesh.Source.get()), _MM_HINT_T0);
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
    FRay LocalRay; double DirectionLength;
    if (!Proxy.Mesh.PrepareRay(Ray, LocalRay, DirectionLength)) return false;
    float LocalDistance = 0.0f;
    if (!Proxy.Box.Intersects(LocalRay.position, LocalRay.direction, LocalDistance) || LocalDistance > static_cast<double>(OutDistance) * DirectionLength) return false;
    if (Proxy.HasCustomBox) {
        const auto It = mLocalBoxes.find(ComponentKey(Proxy.Component.GetHandle()));
        if (It == mLocalBoxes.end() || !It->second.Intersects(LocalRay.position, LocalRay.direction, LocalDistance) || LocalDistance > static_cast<double>(OutDistance) * DirectionLength) return false;
    }
    float HitDistance = static_cast<float>((std::max)(LocalDistance, 0.0f) / DirectionLength);
    if (Proxy.Kind == EProxyKind::Mesh) {
        if (!Proxy.Mesh.RaycastPrepared(LocalRay, DirectionLength, HitDistance, OutDistance)) return false;
    }
    if (HitDistance >= OutDistance) return false;
    OutDistance = HitDistance;
    return true;
}

void FWorldRaycastAccelerationStructure::Clear() {
    mTree.Clear();
    mLocalBoxes.clear();
    mLeaves.clear();
    mFreeLeaves.clear();
    mLeafIndices.clear();
    mBuilt = false;
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
        MakeProxy(Component, Box);
        if (!AreBoundsFinite(Box)) { Clear(); return false; }
        Items.push_back({Box, Ref});
        Bounds.Expand(Box);
    }
    if (Items.empty()) { mBuilt = true; return true; }
    if (Items.size() > (static_cast<std::size_t>(InvalidIndex) + 1) / 2) return false;
    TArray<FNode> BuildNodes;
    BuildNodes.reserve(Items.size() * 2 - 1);
    MakeChild(BuildNodes, Items, 0, static_cast<Uint32>(Items.size()), Bounds);
    BVH8::OptimizeBuildTreelets(BuildNodes);
    mLeaves.reserve(Items.size());
    mTree.Build(BuildNodes, [](const FNode& Node) { return Node.mLeft == InvalidIndex; }, [&](const FNode& Node) {
        const Uint32 Index = static_cast<Uint32>(mLeaves.size());
        DirectX::BoundingBox Box;
        mLeaves.push_back(MakeProxy(Node.Component.Get(), Box));
        mLeafIndices.emplace(ComponentKey(Node.Component.GetHandle()), Index);
        return Index;
    });
    mBuilt = true;
    return true;
}

void FWorldRaycastAccelerationStructure::InsertComponent(UPrimitiveComponent* Component) {
    if (!mBuilt || Component == nullptr) return;
    const Uint64 Key = ComponentKey(Component->GetHandle());
    if (mLeafIndices.contains(Key)) return;
    DirectX::BoundingBox Box;
    auto Proxy = MakeProxy(Component, Box);
    if (!AreBoundsFinite(Box)) { mLocalBoxes.erase(Key); return; }
    const Uint32 Index = mFreeLeaves.empty() ? static_cast<Uint32>(mLeaves.size()) : mFreeLeaves.back();
    if (Index > BVH8::IndexMask) throw std::length_error("World BVH leaf capacity");
    if (mFreeLeaves.empty()) mLeaves.push_back(std::move(Proxy));
    else { mFreeLeaves.pop_back(); mLeaves[Index] = std::move(Proxy); }
    mTree.Insert(Index, Box);
    mLeafIndices.emplace(Key, Index);
}

void FWorldRaycastAccelerationStructure::UpdateComponent(UPrimitiveComponent* Component) {
    if (!mBuilt || Component == nullptr) return;
    const auto It = mLeafIndices.find(ComponentKey(Component->GetHandle()));
    if (It == mLeafIndices.end()) { InsertComponent(Component); return; }
    auto& Proxy = mLeaves[It->second];
    DirectX::BoundingBox Box;
    Proxy = MakeProxy(Component, Box);
    if (!AreBoundsFinite(Box)) { RemoveComponent(Component); return; }
    mTree.Update(It->second, Box);
}

void FWorldRaycastAccelerationStructure::RemoveComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr) return;
    mLocalBoxes.erase(ComponentKey(Component->GetHandle()));
    const auto It = mLeafIndices.find(ComponentKey(Component->GetHandle()));
    if (It == mLeafIndices.end()) return;
    const Uint32 Index = It->second;
    mTree.Remove(Index);
    mLeaves[Index] = {};
    mFreeLeaves.push_back(Index);
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
        BuildNodes[NodeIndex].BoundingBox = Items[First].BoundingBox;
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

bool FWorldRaycastAccelerationStructure::Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld) const {
    OutComponent = nullptr;
    OutDistance = std::numeric_limits<float>::max();
    TArray<Uint32> StaleLeaves;
    for (;;) {
        Uint32 Winner = InvalidIndex;
        OutDistance = std::numeric_limits<float>::max();
        mTree.Raycast(Ray, OutDistance, [&](Uint32 LeafIndex, float& ClosestDistance) {
            if (std::find(StaleLeaves.begin(), StaleLeaves.end(), LeafIndex) != StaleLeaves.end()) return false;
            if (!RaycastProxy(mLeaves[LeafIndex], Ray, ClosestDistance, CameraWorld)) return false;
            Winner = LeafIndex;
            return true;
        });
        if (Winner == InvalidIndex) return false;
        OutComponent = mLeaves[Winner].Component.Get();
        if (OutComponent != nullptr) return true;
        StaleLeaves.push_back(Winner);
    }
}

void FWorldRaycastAccelerationStructure::WarmupRaycast() const {
    (void)std::chrono::steady_clock::now();
    BVH8::FNode Node{}; BVH8::FTrianglePacket Packet{};
    for (Uint32 i = 0; i < 8; ++i) {
        Node.MinX[i] = Node.MinY[i] = -1.0f; Node.MaxX[i] = Node.MaxY[i] = 1.0f;
        Node.MinZ[i] = -0.01f; Node.MaxZ[i] = 0.01f; Node.Children[i] = BVH8::MakeReference(0, 8, true);
        Packet.V0[0][i] = Packet.V0[1][i] = -1.0f; Packet.Edge1[1][i] = 2.0f; Packet.Edge2[0][i] = 2.0f;
    }
    for (bool Parallel : {false, true}) for (bool Reverse : {false, true}) {
        const auto Direction = DirectX::XMVector3Normalize(DirectX::XMVectorSet(Parallel ? 0.0f : 0.1f, Parallel ? 0.0f : 0.1f, Reverse ? -1.0f : 1.0f, 0.0f));
        const FRay Ray{DirectX::XMVectorSubtract(DirectX::XMVectorSet(-0.25f, -0.25f, 0, 0), DirectX::XMVectorScale(Direction, 2.0f)), Direction};
        float Distance = 10.0f;
        BVH8::RaycastTrianglePackets(&Node, BVH8::MakeReference(0, 8), BVH8::FRayData{Ray}, Ray, Distance, &Packet, Reverse);
    }
    const FMatrix Camera = FMatrix::Identity;
    const size_t Count = (std::min)(mLeaves.size(), size_t{16});
    for (size_t i = 0; i < Count; ++i) {
        const auto& Proxy = mLeaves[i * mLeaves.size() / Count];
        if (!Proxy.Enabled) continue;
        auto Center = DirectX::XMLoadFloat3(&Proxy.Box.Center);
        float Radius = (std::max)(Proxy.BillboardSize.mX, Proxy.BillboardSize.mY);
        if (Proxy.Kind != EProxyKind::Billboard) {
            if (!Proxy.Mesh.Valid) continue;
            const auto Scale = DirectX::XMVectorSetW(DirectX::XMLoadFloat3(&Proxy.Mesh.InverseScale), 1.0f), Rotation = DirectX::XMLoadFloat4(&Proxy.Mesh.Rotation);
            Center = DirectX::XMVectorAdd(DirectX::XMVector3Rotate(DirectX::XMVectorDivide(Center, Scale), Rotation), DirectX::XMLoadFloat3(&Proxy.Mesh.Position));
            const auto Extents = DirectX::XMVectorAbs(DirectX::XMVectorDivide(DirectX::XMLoadFloat3(&Proxy.Box.Extents), Scale));
            Radius = (std::max)({DirectX::XMVectorGetX(Extents), DirectX::XMVectorGetY(Extents), DirectX::XMVectorGetZ(Extents)}) * 2.0f;
        }
        if (!std::isfinite(Radius) || Radius > std::numeric_limits<float>::max() * 0.5f || DirectX::XMVector3IsNaN(Center) || DirectX::XMVector3IsInfinite(Center)) continue;
        Radius = (std::max)(Radius, 1.0f);
        for (bool Parallel : {false, true}) {
            const auto Direction = DirectX::XMVector3Normalize(DirectX::XMVectorSet(Parallel ? 0.0f : 0.3f, Parallel ? 0.0f : 0.2f, 1.0f, 0.0f));
            const FRay Ray{DirectX::XMVectorSubtract(Center, DirectX::XMVectorScale(Direction, Radius * 2.0f)), Direction};
            float Distance = std::numeric_limits<float>::max();
            RaycastProxy(Proxy, Ray, Distance, &Camera);
            if (i % 4 == 0) { UPrimitiveComponent* Component = nullptr; Raycast(Ray, Component, Distance, &Camera); }
        }
    }
}

void UPickingSubsystem::RegisterComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr) return;
    const auto Handle = Component->GetHandle();
    const Uint64 Key = (static_cast<Uint64>(Handle.mGeneration) << 32) | Handle.mIndex;
    if (!mRegisteredComponentKeys.insert(Key).second) return;
    mComponents.emplace_back(Component);
    if (RaycastAccelerationStructure.IsBuilt()) UpdateComponent(Component);
}

void UPickingSubsystem::UnregisterComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr) return;
    const auto Handle = Component->GetHandle();
    const Uint64 Key = (static_cast<Uint64>(Handle.mGeneration) << 32) | Handle.mIndex;
    mRegisteredComponentKeys.erase(Key);
    mDirtyComponentKeys.erase(Key);
    std::erase_if(mDirtyComponents, [Handle](const auto& Ref) { return Ref.GetHandle().mIndex == Handle.mIndex && Ref.GetHandle().mGeneration == Handle.mGeneration; });
    RaycastAccelerationStructure.RemoveComponent(Component);
    std::erase_if(mComponents, [Component](const TObjectRef<UPrimitiveComponent>& Ref) { return Ref.Get() == Component; });
}

bool UPickingSubsystem::RebuildAccelerationStructure() {
    if (!RaycastAccelerationStructure.BuildStructure(mComponents)) return false;
    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();
    RaycastAccelerationStructure.WarmupRaycast();
    return true;
}

void UPickingSubsystem::UpdateComponent(UPrimitiveComponent* Component) {
    if (Component == nullptr || !ContainsComponent(Component)) return;
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
    SynchronizeProxies();
    const auto Start = std::chrono::steady_clock::now();
    RaycastAccelerationStructure.Raycast(Ray, OutComponent, OutDistance, CameraWorld);
    const double Milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
    Stat::RecordPickingTime(Milliseconds);
    return OutComponent != nullptr;
}

bool UPickingSubsystem::ContainsComponent(const UPrimitiveComponent* Component) const {
    if (Component == nullptr) return false;
    const auto Handle = Component->GetHandle();
    return mRegisteredComponentKeys.contains((static_cast<Uint64>(Handle.mGeneration) << 32) | Handle.mIndex);
}

const TArray<TObjectRef<UPrimitiveComponent>>& UPickingSubsystem::GetRegisteredComponents() const {
    return mComponents;
}

void UPickingSubsystem::OnDeinitialize() {
    mComponents.clear();
    mRegisteredComponentKeys.clear();
    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();
    RaycastAccelerationStructure.Clear();
}
