#include "pch.h"
#include "FBVHTree.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool FBVHNode::IsLeaf() const {
    return mLeftChild == -1 && mRightChild == -1;
}

void FBVHTree::Build(const TArray<FActorProbe>& Probes) {
    Clear();

    constexpr std::size_t MaximumLeafCount{(static_cast<std::size_t>((std::numeric_limits<Int32>::max)()) + 1) / 2};
    if (Probes.empty() || Probes.size() > MaximumLeafCount) {
        return;
    }

    TArray<DirectX::BoundingBox> Bounds{};
    TArray<Uint32> Indices{};
    Bounds.reserve(Probes.size());
    Indices.reserve(Probes.size());
    for (std::size_t Index{}; Index < Probes.size(); ++Index) {
        Bounds.push_back(Probes[Index].mWorldAABB);
        Indices.push_back(static_cast<Uint32>(Index));
    }

    Build(Bounds, Indices);
}

void FBVHTree::Build(std::span<const DirectX::BoundingBox> Bounds, std::span<const Uint32> Indices) {
    Clear();

    constexpr std::size_t MaximumProbeCount{static_cast<std::size_t>((std::numeric_limits<Int32>::max)()) + 1};
    constexpr std::size_t MaximumLeafCount{MaximumProbeCount / 2};
    if (Indices.empty() || Bounds.empty() || Bounds.size() > MaximumProbeCount || Indices.size() > MaximumLeafCount) {
        return;
    }

    mProbeLeaves.assign(Bounds.size(), -1);
    for (const Uint32 Index : Indices) {
        if (Index >= Bounds.size() || mProbeLeaves[Index] != -1) {
            Clear();
            return;
        }

        mProbeLeaves[Index] = 0;
    }

    std::fill(mProbeLeaves.begin(), mProbeLeaves.end(), -1);
    mIndices.assign(Indices.begin(), Indices.end());
    mNodes.reserve(Indices.size() * 2 - 1);

    mRootIndex = BuildRecursive(Bounds, 0, mIndices.size(), -1);
}

void FBVHTree::Clear() {
    mNodes.clear();
    mIndices.clear();
    mProbeLeaves.clear();
    mRootIndex = -1;
}

const TArray<FBVHNode>& FBVHTree::GetNodes() const {
    return mNodes;
}

void FBVHTree::UpdateBounds(Uint32 ProbeIndex, const DirectX::BoundingBox& Bounds) {
    if (ProbeIndex >= mProbeLeaves.size()) {
        return;
    }

    const Int32 LeafIndex{mProbeLeaves[ProbeIndex]};
    if (LeafIndex != -1) {
        mNodes[LeafIndex].mBounds = Bounds;
    }
}

void FBVHTree::Refit(std::span<const Uint32> ChangedIndices) {
    if (mRootIndex == -1 || ChangedIndices.empty()) {
        return;
    }

    if (ChangedIndices.size() > mIndices.size() / 4) {
        for (std::size_t Index{mNodes.size()}; Index > 0; --Index) {
            FBVHNode& Node{mNodes[Index - 1]};
            if (!Node.IsLeaf()) {
                DirectX::BoundingBox::CreateMerged(Node.mBounds, mNodes[Node.mLeftChild].mBounds, mNodes[Node.mRightChild].mBounds);
            }
        }
        return;
    }

    for (const Uint32 ProbeIndex : ChangedIndices) {
        if (ProbeIndex >= mProbeLeaves.size() || mProbeLeaves[ProbeIndex] == -1) {
            continue;
        }

        Int32 NodeIndex{mNodes[mProbeLeaves[ProbeIndex]].mParentIndex};
        while (NodeIndex != -1) {
            FBVHNode& Node{mNodes[NodeIndex]};
            DirectX::BoundingBox::CreateMerged(Node.mBounds, mNodes[Node.mLeftChild].mBounds, mNodes[Node.mRightChild].mBounds);
            NodeIndex = Node.mParentIndex;
        }
    }
}

void FBVHTree::FrustumCull(const FFrustum& Frustum, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const {
    if (mRootIndex == -1 || Probes.empty()) {
        return;
    }

    CullRecursive(mRootIndex, Frustum, Probes, OutVisibleProbes);
}

void FBVHTree::FrustumCull(const FFrustum& Frustum, TArray<Uint32>& OutIndices) const {
    OutIndices.clear();

    if (mRootIndex == -1) {
        return;
    }

    OutIndices.reserve(mIndices.size());
    CullRecursive(mRootIndex, Frustum, OutIndices);
}

void FBVHTree::FrustumCull(const FMatrix& ViewProjection, TArray<Uint32>& OutIndices) const {
    OutIndices.clear();

    if (mRootIndex == -1) {
        return;
    }

    std::array<DirectX::XMFLOAT4, 6> Planes{};
    for (Uint32 Axis{}; Axis < 3; ++Axis) {
        const float NearScale{Axis == 2 ? 0.0f : 1.0f};
        Planes[Axis * 2] = DirectX::XMFLOAT4{ViewProjection.M[0][3] * NearScale + ViewProjection.M[0][Axis], ViewProjection.M[1][3] * NearScale + ViewProjection.M[1][Axis], ViewProjection.M[2][3] * NearScale + ViewProjection.M[2][Axis], ViewProjection.M[3][3] * NearScale + ViewProjection.M[3][Axis]};
        Planes[Axis * 2 + 1] = DirectX::XMFLOAT4{ViewProjection.M[0][3] - ViewProjection.M[0][Axis], ViewProjection.M[1][3] - ViewProjection.M[1][Axis], ViewProjection.M[2][3] - ViewProjection.M[2][Axis], ViewProjection.M[3][3] - ViewProjection.M[3][Axis]};
    }

    for (DirectX::XMFLOAT4& Plane : Planes) {
        const float Length{std::sqrt(Plane.x * Plane.x + Plane.y * Plane.y + Plane.z * Plane.z)};
        if (Length > 1e-8f && std::isfinite(Length) && std::isfinite(Plane.w)) {
            const float InverseLength{1.0f / Length};
            Plane.x *= InverseLength;
            Plane.y *= InverseLength;
            Plane.z *= InverseLength;
            Plane.w *= InverseLength;
        } else {
            Plane = {};
        }
    }

    OutIndices.reserve(mIndices.size());
    CullRecursive(mRootIndex, Planes, OutIndices);
}

Int32 FBVHTree::BuildRecursive(std::span<const DirectX::BoundingBox> Bounds, std::size_t Start, std::size_t End, Int32 ParentIndex) {
    const std::size_t Count{End - Start};
    if (Count == 0) {
        return -1;
    }

    const Int32 NodeIndex{static_cast<Int32>(mNodes.size())};
    mNodes.emplace_back();
    mNodes[NodeIndex].mParentIndex = ParentIndex;
    mNodes[NodeIndex].mFirstIndex = static_cast<Uint32>(Start);
    mNodes[NodeIndex].mIndexCount = static_cast<Uint32>(Count);

    if (Count == 1) {
        const Uint32 ProbeIndex{mIndices[Start]};
        mNodes[NodeIndex].mBounds = Bounds[ProbeIndex];
        mNodes[NodeIndex].mProbeIndex = static_cast<Int32>(ProbeIndex);
        mProbeLeaves[ProbeIndex] = NodeIndex;
        return NodeIndex;
    }

    DirectX::XMFLOAT3 MinCenter{FLT_MAX, FLT_MAX, FLT_MAX};
    DirectX::XMFLOAT3 MaxCenter{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (std::size_t Index{Start}; Index < End; ++Index) {
        const DirectX::XMFLOAT3& Center{Bounds[mIndices[Index]].Center};
        MinCenter.x = (std::min)(MinCenter.x, Center.x);
        MinCenter.y = (std::min)(MinCenter.y, Center.y);
        MinCenter.z = (std::min)(MinCenter.z, Center.z);
        MaxCenter.x = (std::max)(MaxCenter.x, Center.x);
        MaxCenter.y = (std::max)(MaxCenter.y, Center.y);
        MaxCenter.z = (std::max)(MaxCenter.z, Center.z);
    }

    Uint32 Axis{};
    const float ExtentX{MaxCenter.x - MinCenter.x};
    const float ExtentY{MaxCenter.y - MinCenter.y};
    const float ExtentZ{MaxCenter.z - MinCenter.z};
    if (ExtentY > ExtentX && ExtentY > ExtentZ) {
        Axis = 1;
    } else if (ExtentZ > ExtentX && ExtentZ > ExtentY) {
        Axis = 2;
    }

    const std::size_t Mid{Start + Count / 2};
    std::nth_element(mIndices.begin() + Start, mIndices.begin() + Mid, mIndices.begin() + End, [Bounds, Axis](Uint32 Left, Uint32 Right) {
        const DirectX::XMFLOAT3& CenterA{Bounds[Left].Center};
        const DirectX::XMFLOAT3& CenterB{Bounds[Right].Center};
        if (Axis == 0) {
            return CenterA.x < CenterB.x;
        }
        if (Axis == 1) {
            return CenterA.y < CenterB.y;
        }
        return CenterA.z < CenterB.z;
    });

    const Int32 Left{BuildRecursive(Bounds, Start, Mid, NodeIndex)};
    const Int32 Right{BuildRecursive(Bounds, Mid, End, NodeIndex)};
    mNodes[NodeIndex].mLeftChild = Left;
    mNodes[NodeIndex].mRightChild = Right;
    DirectX::BoundingBox::CreateMerged(mNodes[NodeIndex].mBounds, mNodes[Left].mBounds, mNodes[Right].mBounds);
    return NodeIndex;
}

void FBVHTree::CullRecursive(Int32 NodeIndex, const FFrustum& Frustum, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const {
    if (NodeIndex == -1) {
        return;
    }

    const FBVHNode& Node{mNodes[NodeIndex]};
    const DirectX::ContainmentType Containment{Frustum.Contains(Node.mBounds)};
    if (Containment == DirectX::DISJOINT) {
        return;
    }

    if (Containment == DirectX::CONTAINS) {
        CollectAllLeaves(NodeIndex, Probes, OutVisibleProbes);
        return;
    }

    if (Node.IsLeaf()) {
        if (static_cast<std::size_t>(Node.mProbeIndex) >= Probes.size()) {
            return;
        }

        const FActorProbe& Probe{Probes[Node.mProbeIndex]};
        if (Frustum.Intersects(Probe.mWorldOBB)) {
            OutVisibleProbes.push_back(Probe);
        }
    } else {
        CullRecursive(Node.mLeftChild, Frustum, Probes, OutVisibleProbes);
        CullRecursive(Node.mRightChild, Frustum, Probes, OutVisibleProbes);
    }
}

void FBVHTree::CullRecursive(Int32 NodeIndex, const FFrustum& Frustum, TArray<Uint32>& OutIndices) const {
    if (NodeIndex == -1) {
        return;
    }

    const FBVHNode& Node{mNodes[NodeIndex]};
    const DirectX::ContainmentType Containment{Frustum.Contains(Node.mBounds)};
    if (Containment == DirectX::DISJOINT) {
        return;
    }

    if (Containment == DirectX::CONTAINS) {
        CollectAllLeaves(NodeIndex, OutIndices);
    } else if (Node.IsLeaf()) {
        OutIndices.push_back(static_cast<Uint32>(Node.mProbeIndex));
    } else {
        CullRecursive(Node.mLeftChild, Frustum, OutIndices);
        CullRecursive(Node.mRightChild, Frustum, OutIndices);
    }
}

void FBVHTree::CullRecursive(Int32 NodeIndex, const std::array<DirectX::XMFLOAT4, 6>& Planes, TArray<Uint32>& OutIndices) const {
    if (NodeIndex == -1) {
        return;
    }

    const FBVHNode& Node{mNodes[NodeIndex]};
    const DirectX::XMFLOAT3& Center{Node.mBounds.Center};
    const DirectX::XMFLOAT3& Extents{Node.mBounds.Extents};

    bool Contained{true};
    constexpr float Epsilon{1e-5f};
    for (const DirectX::XMFLOAT4& Plane : Planes) {
        const float Distance{Plane.x * Center.x + Plane.y * Center.y + Plane.z * Center.z + Plane.w};
        const float Radius{std::abs(Plane.x) * Extents.x + std::abs(Plane.y) * Extents.y + std::abs(Plane.z) * Extents.z};
        if (Distance + Radius < -Epsilon) {
            return;
        }

        if (Distance - Radius < Epsilon) {
            Contained = false;
        }
    }

    if (Contained) {
        CollectAllLeaves(NodeIndex, OutIndices);
    } else if (Node.IsLeaf()) {
        OutIndices.push_back(static_cast<Uint32>(Node.mProbeIndex));
    } else {
        CullRecursive(Node.mLeftChild, Planes, OutIndices);
        CullRecursive(Node.mRightChild, Planes, OutIndices);
    }
}

void FBVHTree::CollectAllLeaves(Int32 NodeIndex, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const {
    if (NodeIndex == -1) {
        return;
    }

    const FBVHNode& Node{mNodes[NodeIndex]};
    if (Node.IsLeaf()) {
        if (static_cast<std::size_t>(Node.mProbeIndex) < Probes.size()) {
            OutVisibleProbes.push_back(Probes[Node.mProbeIndex]);
        }
        return;
    }

    CollectAllLeaves(Node.mLeftChild, Probes, OutVisibleProbes);
    CollectAllLeaves(Node.mRightChild, Probes, OutVisibleProbes);
}

void FBVHTree::CollectAllLeaves(Int32 NodeIndex, TArray<Uint32>& OutIndices) const {
    if (NodeIndex == -1) {
        return;
    }

    const FBVHNode& Node{mNodes[NodeIndex]};
    OutIndices.insert(OutIndices.end(), mIndices.begin() + Node.mFirstIndex, mIndices.begin() + Node.mFirstIndex + Node.mIndexCount);
}
