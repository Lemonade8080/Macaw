#pragma once

#include "FBVH8.h"

namespace BVH8 {
    template<class TPreparedRay, bool HasParallel, class TVisitLeaf> bool Traverse(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, TVisitLeaf VisitLeaf) {
        const TPreparedRay Prepared{Ray};
        struct FEntry { Uint32 Reference; float Distance; };
        FEntry Stack[64];
        Uint32 StackSize = 0;
        TArray<FEntry> Overflow;
        const auto Push = [&](FEntry Entry) {
            if (StackSize < std::size(Stack)) Stack[StackSize++] = Entry;
            else Overflow.push_back(Entry);
        };
        FEntry Entry{RootReference, 0.0f};
        bool Hit = false;
        for (;;) {
            if (Entry.Distance <= ClosestDistance) {
                if (Entry.Reference & BVH8::LeafBit) {
                    Hit |= VisitLeaf(Entry.Reference & BVH8::IndexMask, BVH8::GetCount(Entry.Reference), ClosestDistance);
                } else {
                    const auto& Node = Nodes[Entry.Reference & BVH8::IndexMask];
                    _mm_prefetch(reinterpret_cast<const char*>(Node.Children), _MM_HINT_T0);
                    float Distances[8];
                    Uint32 Mask = Prepared.template Intersect<HasParallel>(Node, ClosestDistance, Distances, BVH8::GetCount(Entry.Reference));
                    if (Mask != 0 && (Mask & (Mask - 1)) == 0) {
                        const Uint32 Lane = std::countr_zero(Mask);
                        Entry = {Node.Children[Lane], Distances[Lane]};
                        continue;
                    }
                    const Uint32 Remaining = Mask & (Mask - 1);
                    if (Remaining != 0 && (Remaining & (Remaining - 1)) == 0) {
                        const Uint32 A = std::countr_zero(Mask), B = std::countr_zero(Remaining);
                        const FEntry First{Node.Children[A], Distances[A]}, Second{Node.Children[B], Distances[B]};
                        if (First.Distance <= Second.Distance) { Push(Second); Entry = First; }
                        else { Push(First); Entry = Second; }
                        continue;
                    }
                    FEntry Children[8];
                    Uint32 Count = 0;
                    while (Mask != 0) {
                        const Uint32 Lane = std::countr_zero(Mask);
                        Mask &= Mask - 1;
                        const FEntry Child{Node.Children[Lane], Distances[Lane]};
                        Uint32 Position = Count++;
                        while (Position > 0 && Child.Distance < Children[Position - 1].Distance) { Children[Position] = Children[Position - 1]; --Position; }
                        Children[Position] = Child;
                    }
                    if (Count != 0) {
                        for (Uint32 i = Count; i > 1; --i) Push(Children[i - 1]);
                        Entry = Children[0];
                        continue;
                    }
                }
            }
            if (!Overflow.empty()) { Entry = Overflow.back(); Overflow.pop_back(); }
            else if (StackSize != 0) Entry = Stack[--StackSize];
            else break;
        }
        return Hit;
    }
}
