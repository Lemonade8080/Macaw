#pragma once
#include "Core/Common.h"
#include "Math/FMath.h"
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace BVH8 {
    inline double BuildArea(const DirectX::BoundingBox& Box) { return double(Box.Extents.x) * Box.Extents.y + double(Box.Extents.y) * Box.Extents.z + double(Box.Extents.z) * Box.Extents.x; }
    inline DirectX::BoundingBox MergeBuildBounds(const DirectX::BoundingBox& A, const DirectX::BoundingBox& B) {
        const float AC[]{A.Center.x, A.Center.y, A.Center.z}, AE[]{A.Extents.x, A.Extents.y, A.Extents.z}, BC[]{B.Center.x, B.Center.y, B.Center.z}, BE[]{B.Extents.x, B.Extents.y, B.Extents.z};
        float Center[3], Extents[3];
        for (Uint32 Axis = 0; Axis < 3; ++Axis) {
            const float Min = (std::min)(AC[Axis] - AE[Axis], BC[Axis] - BE[Axis]), Max = (std::max)(AC[Axis] + AE[Axis], BC[Axis] + BE[Axis]);
            Center[Axis] = Min * 0.5f + Max * 0.5f;
            Extents[Axis] = std::nextafter((std::max)(Center[Axis] - Min, Max - Center[Axis]), std::numeric_limits<float>::infinity());
        }
        return {{Center[0], Center[1], Center[2]}, {Extents[0], Extents[1], Extents[2]}};
    }

    template<class TNode> double EstimateWideBuildCost(const TArray<TNode>& Nodes) {
        const auto Evaluate = [&](auto&& Self, Uint32 Index) -> std::array<double, 9> {
            std::array<double, 9> Cost; Cost.fill(std::numeric_limits<double>::infinity());
            const auto& Node = Nodes[Index];
            if (Node.mLeft == std::numeric_limits<Uint32>::max()) { Cost[1] = 0; return Cost; }
            const auto Left = Self(Self, Node.mLeft), Right = Self(Self, Node.mRight);
            double Best = std::numeric_limits<double>::infinity();
            for (Uint32 Count = 2; Count <= 8; ++Count) {
                for (Uint32 Split = 1; Split < Count; ++Split) Cost[Count] = (std::min)(Cost[Count], Left[Split] + Right[Count - Split]);
                Best = (std::min)(Best, Cost[Count]);
            }
            Cost[1] = BuildArea(Node.BoundingBox) + Best;
            return Cost;
        };
        return Nodes.empty() ? 0.0 : Evaluate(Evaluate, 0)[1];
    }

    template<class TNode> bool OptimizeBuildTreelets(TArray<TNode>& Nodes) {
        if (Nodes.size() < 15) return false;
        const double OriginalCost = EstimateWideBuildCost(Nodes);
        if (!std::isfinite(OriginalCost) || OriginalCost <= 0) return false;
        TArray<TNode> Candidate = Nodes;
        TArray<double> Costs(Nodes.size());
        const auto Optimize = [&](auto&& Self, Uint32 Root) -> void {
            auto& Node = Candidate[Root];
            if (Node.mLeft == std::numeric_limits<Uint32>::max()) { Costs[Root] = 0; return; }
            Self(Self, Node.mLeft); Self(Self, Node.mRight);
            Node.BoundingBox = MergeBuildBounds(Candidate[Node.mLeft].BoundingBox, Candidate[Node.mRight].BoundingBox);
            Costs[Root] = BuildArea(Node.BoundingBox) + Costs[Node.mLeft] + Costs[Node.mRight];
            Uint32 Frontier[7]{Root}, Slots[6]{}; Uint32 Count = 1, SlotCount = 0;
            while (Count < 7) {
                Uint32 Best = Count; double Largest = -1;
                for (Uint32 i = 0; i < Count; ++i) if (Candidate[Frontier[i]].mLeft != std::numeric_limits<Uint32>::max() && BuildArea(Candidate[Frontier[i]].BoundingBox) > Largest) { Best = i; Largest = BuildArea(Candidate[Frontier[i]].BoundingBox); }
                if (Best == Count) break;
                const Uint32 Index = Frontier[Best]; Slots[SlotCount++] = Index;
                Frontier[Best] = Candidate[Index].mLeft; Frontier[Count++] = Candidate[Index].mRight;
            }
            if (Count < 3) return;
            DirectX::BoundingBox Bounds[128]; double Best[128]{}; Uint32 Split[128]{};
            const Uint32 Full = (1u << Count) - 1;
            for (Uint32 Mask = 1; Mask <= Full; ++Mask) {
                const Uint32 Bit = Mask & (~Mask + 1), Rest = Mask ^ Bit, Index = Frontier[std::countr_zero(Bit)];
                if (Rest == 0) { Bounds[Mask] = Candidate[Index].BoundingBox; Best[Mask] = Costs[Index]; continue; }
                Bounds[Mask] = MergeBuildBounds(Bounds[Bit], Bounds[Rest]); Best[Mask] = std::numeric_limits<double>::infinity();
                const double Area = BuildArea(Bounds[Mask]);
                for (Uint32 Left = (Mask - 1) & Mask; Left != 0; Left = (Left - 1) & Mask) {
                    if ((Left & Bit) == 0) continue;
                    const double Cost = Best[Left] + Best[Mask ^ Left] + Area;
                    if (Cost < Best[Mask]) { Best[Mask] = Cost; Split[Mask] = Left; }
                }
            }
            if (Best[Full] >= Costs[Root] * (1.0 - 1e-6)) return;
            Uint32 Next = 0;
            const auto Rebuild = [&](auto&& Recur, Uint32 Mask) -> Uint32 {
                if (std::has_single_bit(Mask)) return Frontier[std::countr_zero(Mask)];
                const Uint32 Index = Slots[Next++], Left = Recur(Recur, Split[Mask]), Right = Recur(Recur, Mask ^ Split[Mask]);
                Candidate[Index].mLeft = Left; Candidate[Index].mRight = Right; Candidate[Index].BoundingBox = MergeBuildBounds(Candidate[Left].BoundingBox, Candidate[Right].BoundingBox); Costs[Index] = BuildArea(Candidate[Index].BoundingBox) + Costs[Left] + Costs[Right];
                return Index;
            };
            Rebuild(Rebuild, Full);
        };
        Optimize(Optimize, 0);
        if (!(EstimateWideBuildCost(Candidate) < OriginalCost * 0.99)) return false;
        Nodes.swap(Candidate);
        return true;
    }
}
