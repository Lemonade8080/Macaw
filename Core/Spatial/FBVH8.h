#pragma once

#include "Core/Common.h"
#include "Math/FMath.h"
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace BVH8 {
    constexpr Uint32 LeafBit = 0x80000000u;
    constexpr Uint32 CountShift = 28;
    constexpr Uint32 IndexMask = (1u << CountShift) - 1;
    constexpr Uint32 GetCount(Uint32 Reference) { return ((Reference >> CountShift) & 7u) + 1; }
    constexpr Uint32 GetValidMask(Uint32 Reference) { return (1u << GetCount(Reference)) - 1; }
    inline Uint32 MakeReference(Uint32 Index, Uint32 Count, bool Leaf = false) {
        if (Index > IndexMask || Count == 0 || Count > 8) throw std::out_of_range("BVH8 reference capacity");
        return (Leaf ? LeafBit : 0u) | ((Count - 1) << CountShift) | Index;
    }
    struct FLeaf { Uint32 Index, Count; };
    inline Uint32 MakeLeafReference(FLeaf Leaf) { return MakeReference(Leaf.Index, Leaf.Count, true); }
    inline Uint32 MakeLeafReference(Uint32 Index) { return MakeReference(Index, 1, true); }

    struct alignas(64) FNode {
        float MinX[8]{}, MaxX[8]{};
        float MinY[8]{}, MaxY[8]{};
        float MinZ[8]{}, MaxZ[8]{};
        Uint32 Children[8]{};
    };
    static_assert(sizeof(FNode) == 256 && alignof(FNode) == 64);

    struct FRayData {
        float Origin[3];
        float InverseDirection[3];
        bool Parallel[3];
        explicit FRayData(const FRay& Ray);
    };
    using FVisitLeaf = bool (*)(void*, Uint32, float&);
    using FRaycaster = bool (*)(const FNode*, Uint32, const FRayData&, float&, void*, FVisitLeaf);
    struct FTriangle;
    struct FTrianglePacket;
    using FTriangleRaycaster = bool (*)(const FNode*, Uint32, const FRayData&, const FRay&, float&, const FTriangle*, bool);
    using FTrianglePacketRaycaster = bool (*)(const FNode*, Uint32, const FRayData&, const FRay&, float&, const FTrianglePacket*, bool);
    void Initialize();
    bool Raycast(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf);
    bool RaycastSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf);
    bool RaycastAVX(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf);
    bool SupportsAVX();
    bool RaycastTrianglePackets(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding = false);
    bool RaycastTrianglePacketsSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding = false);
    bool RaycastTrianglePacketsAVX(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding = false);
    bool RaycastTriangles(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTriangle* Triangles, bool ReverseWinding = false);
    bool RaycastTrianglesSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTriangle* Triangles, bool ReverseWinding = false);
    bool RaycastTrianglesAVX(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTriangle* Triangles, bool ReverseWinding = false);
    Uint32 IntersectSSE(const FNode& Node, const FRayData& Ray, float MaxDistance, float* EntryDistances, Uint32 Count);
    Uint32 IntersectAVX(const FNode& Node, const FRayData& Ray, float MaxDistance, float* EntryDistances, Uint32 Count);
}

class FBVH8 {
public:
    void Clear() { mNodes.clear(); mRootReference = 0; }
    Uint32 GetRootReference() const { return mRootReference; }
    DirectX::BoundingBox RefitChild(Uint32 Reference, Uint32 Lane, const DirectX::BoundingBox& Box);
    const TArray<BVH8::FNode>& GetNodes() const { return mNodes; }
    bool RaycastTrianglePackets(const FRay& Ray, float& ClosestDistance, const BVH8::FTrianglePacket* Packets, bool ReverseWinding = false) const {
        if (mNodes.empty() || !(ClosestDistance >= 0.0f)) return false;
        return BVH8::RaycastTrianglePackets(mNodes.data(), mRootReference, BVH8::FRayData{Ray}, Ray, ClosestDistance, Packets, ReverseWinding);
    }
    bool RaycastTriangles(const FRay& Ray, float& ClosestDistance, const BVH8::FTriangle* Triangles, bool ReverseWinding = false) const {
        if (mNodes.empty() || !(ClosestDistance >= 0.0f)) return false;
        return BVH8::RaycastTriangles(mNodes.data(), mRootReference, BVH8::FRayData{Ray}, Ray, ClosestDistance, Triangles, ReverseWinding);
    }

    template<class TNode, class TIsLeaf, class TAddLeaf> void Build(const TArray<TNode>& BinaryNodes, TIsLeaf IsLeaf, TAddLeaf AddLeaf) {
        Build(BinaryNodes, IsLeaf, AddLeaf, [](const TNode&) { return false; });
    }

    template<class TNode, class TIsLeaf, class TAddLeaf, class TCanMakeLeaf> void Build(const TArray<TNode>& BinaryNodes, TIsLeaf IsLeaf, TAddLeaf AddLeaf, TCanMakeLeaf CanMakeLeaf, double LeafCost = 1.0) {
        Clear();
        if (BinaryNodes.empty()) return;
        const auto Area = [](const DirectX::BoundingBox& Box) { return static_cast<double>(Box.Extents.x) * Box.Extents.y + static_cast<double>(Box.Extents.y) * Box.Extents.z + static_cast<double>(Box.Extents.z) * Box.Extents.x; };
        struct FPlan { std::array<double, 9> Cost; std::array<Uint8, 9> LeftCount{}; Uint8 BestCount = 1; };
        TArray<FPlan> Plans(BinaryNodes.size());
        const auto Evaluate = [&](auto&& Self, Uint32 Index) -> void {
            const auto& Node = BinaryNodes[Index];
            auto& Plan = Plans[Index];
            Plan.Cost.fill(std::numeric_limits<double>::infinity());
            if (IsLeaf(Node)) { Plan.Cost[1] = Area(Node.BoundingBox) * LeafCost; return; }
            Self(Self, Node.mLeft);
            Self(Self, Node.mRight);
            double BestCost = std::numeric_limits<double>::infinity();
            for (Uint8 Count = 2; Count <= 8; ++Count) {
                for (Uint8 Left = 1; Left < Count; ++Left) {
                    const double Cost = Plans[Node.mLeft].Cost[Left] + Plans[Node.mRight].Cost[Count - Left];
                    if (Cost < Plan.Cost[Count]) { Plan.Cost[Count] = Cost; Plan.LeftCount[Count] = Left; }
                }
                if (Plan.LeftCount[Count] != 0 && Plan.Cost[Count] <= BestCost) { BestCost = Plan.Cost[Count]; Plan.BestCount = Count; }
            }
            Plan.Cost[1] = Area(Node.BoundingBox) + BestCost;
            if (CanMakeLeaf(Node) && Area(Node.BoundingBox) * LeafCost <= Plan.Cost[1]) { Plan.Cost[1] = Area(Node.BoundingBox) * LeafCost; Plan.BestCount = 1; }
        };
        Evaluate(Evaluate, 0);
        const auto Gather = [&](auto&& Self, Uint32 Index, Uint8 Count, Uint32* Frontier, Uint32& Size) -> void {
            if (Count == 1) { Frontier[Size++] = Index; return; }
            const auto& Node = BinaryNodes[Index];
            const Uint8 Left = Plans[Index].LeftCount[Count];
            Self(Self, Node.mLeft, Left, Frontier, Size);
            Self(Self, Node.mRight, Count - Left, Frontier, Size);
        };
        const auto Collapse = [&](auto&& Self, Uint32 BinaryIndex) -> Uint32 {
            Uint32 Frontier[8]{}, Count = 0;
            Gather(Gather, BinaryIndex, Plans[BinaryIndex].BestCount, Frontier, Count);
            const Uint32 Index = static_cast<Uint32>(mNodes.size());
            if (mNodes.size() > BVH8::IndexMask) throw std::length_error("BVH8 node capacity");
            mNodes.emplace_back();
            for (Uint32 i = 0; i < Count; ++i) {
                const auto& Source = BinaryNodes[Frontier[i]];
                const Uint32 Child = Plans[Frontier[i]].BestCount == 1 ? BVH8::MakeLeafReference(AddLeaf(Source)) : Self(Self, Frontier[i]);
                auto& Node = mNodes[Index];
                const auto& Box = Source.BoundingBox;
                Node.MinX[i] = Box.Center.x - Box.Extents.x; Node.MaxX[i] = Box.Center.x + Box.Extents.x;
                Node.MinY[i] = Box.Center.y - Box.Extents.y; Node.MaxY[i] = Box.Center.y + Box.Extents.y;
                Node.MinZ[i] = Box.Center.z - Box.Extents.z; Node.MaxZ[i] = Box.Center.z + Box.Extents.z;
                Node.Children[i] = Child;
            }
            return BVH8::MakeReference(Index, Count);
        };
        mRootReference = Collapse(Collapse, 0);
    }

    template<class TVisitLeaf> bool Raycast(const FRay& Ray, float& ClosestDistance, TVisitLeaf VisitLeaf) const {
        if (mNodes.empty() || !(ClosestDistance >= 0.0f)) return false;
        const BVH8::FRayData RayData{Ray};
        return BVH8::Raycast(mNodes.data(), mRootReference, RayData, ClosestDistance, &VisitLeaf, [](void* Context, Uint32 Leaf, float& Distance) {
            return (*static_cast<TVisitLeaf*>(Context))(Leaf, Distance);
        });
    }

private:
    TArray<BVH8::FNode> mNodes;
    Uint32 mRootReference = 0;
};
