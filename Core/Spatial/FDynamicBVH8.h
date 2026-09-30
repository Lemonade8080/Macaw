#pragma once

#include "FBVH8.h"
#include <utility>

class FDynamicBVH8 : public FBVH8 {
    friend struct FDynamicBVH8Tests;
public:
    void Clear();
    template<class... TArgs> void Build(TArgs&&... Args) { Clear(); FBVH8::Build(std::forward<TArgs>(Args)...); InitializeParents(); }
    void Insert(Uint32 Leaf, const DirectX::BoundingBox& Box);
    void Remove(Uint32 Leaf);
    void Update(Uint32 Leaf, const DirectX::BoundingBox& Box);
private:
    static constexpr Uint32 Invalid = UINT32_MAX;
    struct FBounds {
        float Min[3]{}, Max[3]{};
        double Area() const;
        static FBounds Merge(const FBounds& A, const FBounds& B);
        static FBounds FromBox(const DirectX::BoundingBox& Box);
        bool operator==(const FBounds&) const = default;
    };
    struct FParent { Uint32 Node = Invalid, Lane = 0; };
    struct FCandidate { Uint32 Ref; FParent Parent; FBounds Bounds; double Inherited, CombinedArea, Lower; Uint32 Depth; };
    struct FInsertion { FCandidate Candidate; double Cost; bool Append; };
    TArray<FParent> mParents, mLeafParents;
    TArray<FBounds> mBounds;
    TArray<Uint8> mCounts;
    TArray<Uint32> mHeights;
    TArray<Uint32> mFreeNodes;
    TArray<FCandidate> mCandidates;
    FInsertion FindInsertion(const FBounds& LeafBounds);
    void InitializeParents();
    Uint32 AllocateNode();
    void FreeNode(Uint32 Node);
    Uint32 Reference(Uint32 Node) const { return BVH8::MakeReference(Node, mCounts[Node]); }
    FBounds ReadBounds(Uint32 Node, Uint32 Lane) const;
    void WriteChild(Uint32 Node, Uint32 Lane, Uint32 Child, const FBounds& Bounds);
    void Refresh(Uint32 Node);
    void RefitAncestors(Uint32 Node);
    bool Rotate(Uint32 Node);
};
