#include "pch.h"
#include "FDynamicBVH8.h"
#include <cmath>

double FDynamicBVH8::FBounds::Area() const {
    const double X = static_cast<double>(Max[0]) - Min[0], Y = static_cast<double>(Max[1]) - Min[1], Z = static_cast<double>(Max[2]) - Min[2];
    return 2.0 * (X * Y + Y * Z + Z * X);
}

FDynamicBVH8::FBounds FDynamicBVH8::FBounds::Merge(const FBounds& A, const FBounds& B) {
    FBounds Result;
    for (Uint32 i = 0; i < 3; ++i) { Result.Min[i] = (std::min)(A.Min[i], B.Min[i]); Result.Max[i] = (std::max)(A.Max[i], B.Max[i]); }
    return Result;
}

FDynamicBVH8::FBounds FDynamicBVH8::FBounds::FromBox(const DirectX::BoundingBox& Box) {
    FBounds Result;
    const float C[]{Box.Center.x, Box.Center.y, Box.Center.z}, E[]{Box.Extents.x, Box.Extents.y, Box.Extents.z};
    for (Uint32 i = 0; i < 3; ++i) {
        Result.Min[i] = C[i] - E[i]; Result.Max[i] = C[i] + E[i];
        if (!std::isfinite(Result.Min[i]) || !std::isfinite(Result.Max[i]) || E[i] < 0) throw std::invalid_argument("Invalid dynamic BVH bounds");
    }
    return Result;
}

void FDynamicBVH8::Clear() {
    FBVH8::Clear(); mParents.clear(); mLeafParents.clear(); mBounds.clear(); mCounts.clear(); mHeights.clear(); mFreeNodes.clear(); mCandidates.clear();
}

FDynamicBVH8::FBounds FDynamicBVH8::ReadBounds(Uint32 Index, Uint32 Lane) const {
    const auto& Node = mNodes[Index];
    return {{Node.MinX[Lane], Node.MinY[Lane], Node.MinZ[Lane]}, {Node.MaxX[Lane], Node.MaxY[Lane], Node.MaxZ[Lane]}};
}

void FDynamicBVH8::WriteChild(Uint32 Index, Uint32 Lane, Uint32 Child, const FBounds& Bounds) {
    auto& Node = mNodes[Index];
    Node.MinX[Lane] = Bounds.Min[0]; Node.MinY[Lane] = Bounds.Min[1]; Node.MinZ[Lane] = Bounds.Min[2];
    Node.MaxX[Lane] = Bounds.Max[0]; Node.MaxY[Lane] = Bounds.Max[1]; Node.MaxZ[Lane] = Bounds.Max[2]; Node.Children[Lane] = Child;
    const Uint32 ChildIndex = Child & BVH8::IndexMask;
    if (Child & BVH8::LeafBit) {
        if (mLeafParents.size() <= ChildIndex) mLeafParents.resize(static_cast<size_t>(ChildIndex) + 1);
        mLeafParents[ChildIndex] = {Index, Lane};
    } else mParents[ChildIndex] = {Index, Lane};
}

void FDynamicBVH8::InitializeParents() {
    mParents.resize(mNodes.size()); mBounds.resize(mNodes.size()); mCounts.resize(mNodes.size()); mHeights.resize(mNodes.size());
    if (mNodes.empty()) return;
    const auto Visit = [&](auto&& Self, Uint32 Ref) -> void {
        const Uint32 Index = Ref & BVH8::IndexMask, Count = BVH8::GetCount(Ref);
        mCounts[Index] = static_cast<Uint8>(Count);
        mBounds[Index] = ReadBounds(Index, 0);
        for (Uint32 Lane = 0; Lane < Count; ++Lane) {
            const Uint32 Child = mNodes[Index].Children[Lane]; const auto Bounds = ReadBounds(Index, Lane);
            WriteChild(Index, Lane, Child, Bounds); mBounds[Index] = FBounds::Merge(mBounds[Index], Bounds);
            if (!(Child & BVH8::LeafBit)) Self(Self, Child);
        }
        Refresh(Index);
    };
    Visit(Visit, mRootReference);
}

Uint32 FDynamicBVH8::AllocateNode() {
    if (!mFreeNodes.empty()) {
        const Uint32 Index = mFreeNodes.back(); mFreeNodes.pop_back();
        mNodes[Index] = {}; mParents[Index] = {}; mCounts[Index] = 0; mBounds[Index] = {}; mHeights[Index] = 0; return Index;
    }
    if (mNodes.size() > BVH8::IndexMask) throw std::length_error("BVH8 node capacity");
    const Uint32 Index = static_cast<Uint32>(mNodes.size());
    mNodes.emplace_back(); mParents.emplace_back(); mCounts.push_back(0); mBounds.emplace_back(); mHeights.push_back(0);
    return Index;
}

void FDynamicBVH8::FreeNode(Uint32 Index) {
    mNodes[Index] = {}; mParents[Index] = {}; mCounts[Index] = 0; mBounds[Index] = {}; mHeights[Index] = 0; mFreeNodes.push_back(Index);
}

void FDynamicBVH8::Refresh(Uint32 Index) {
    auto Bounds = ReadBounds(Index, 0);
    for (Uint32 Lane = 1; Lane < mCounts[Index]; ++Lane) Bounds = FBounds::Merge(Bounds, ReadBounds(Index, Lane));
    mBounds[Index] = Bounds;
    mHeights[Index] = 1;
    for (Uint32 Lane = 0; Lane < mCounts[Index]; ++Lane) {
        const Uint32 Child = mNodes[Index].Children[Lane];
        if (!(Child & BVH8::LeafBit)) mHeights[Index] = (std::max)(mHeights[Index], mHeights[Child & BVH8::IndexMask] + 1);
    }
    const auto Parent = mParents[Index];
    if (Parent.Node == Invalid) mRootReference = Reference(Index);
    else WriteChild(Parent.Node, Parent.Lane, Reference(Index), Bounds);
}

bool FDynamicBVH8::Rotate(Uint32 Index) {
    double BestGain = 0; Uint32 BestChild = Invalid, BestSibling = 0, BestGrandchild = 0;
    for (Uint32 Lane = 0; Lane < mCounts[Index]; ++Lane) {
        const Uint32 Ref = mNodes[Index].Children[Lane];
        if (Ref & BVH8::LeafBit) continue;
        const Uint32 Child = Ref & BVH8::IndexMask;
        FBounds Prefix[8], Suffix[8]; const Uint32 Count = mCounts[Child];
        Prefix[0] = ReadBounds(Child, 0); Suffix[Count - 1] = ReadBounds(Child, Count - 1);
        for (Uint32 i = 1; i < Count; ++i) Prefix[i] = FBounds::Merge(Prefix[i - 1], ReadBounds(Child, i));
        for (Uint32 i = Count - 1; i > 0; --i) Suffix[i - 1] = FBounds::Merge(Suffix[i], ReadBounds(Child, i - 1));
        for (Uint32 Grandchild = 0; Grandchild < mCounts[Child]; ++Grandchild) {
            const auto Others = Grandchild == 0 ? Suffix[1] : Grandchild + 1 == Count ? Prefix[Count - 2] : FBounds::Merge(Prefix[Grandchild - 1], Suffix[Grandchild + 1]);
            for (Uint32 Sibling = 0; Sibling < mCounts[Index]; ++Sibling) {
                if (Sibling == Lane) continue;
                const auto Bounds = FBounds::Merge(Others, ReadBounds(Index, Sibling));
                const double Gain = mBounds[Child].Area() - Bounds.Area();
                if (Gain > BestGain) { BestGain = Gain; BestChild = Child; BestSibling = Sibling; BestGrandchild = Grandchild; }
            }
        }
    }
    if (BestChild == Invalid) return false;
    const Uint32 A = mNodes[Index].Children[BestSibling], B = mNodes[BestChild].Children[BestGrandchild];
    const auto BoundsA = ReadBounds(Index, BestSibling), BoundsB = ReadBounds(BestChild, BestGrandchild);
    WriteChild(Index, BestSibling, B, BoundsB); WriteChild(BestChild, BestGrandchild, A, BoundsA);
    Refresh(BestChild);
    return true;
}

void FDynamicBVH8::RefitAncestors(Uint32 Index) {
    while (Index != Invalid) { Refresh(Index); if (Rotate(Index)) Refresh(Index); Index = mParents[Index].Node; }
}

FDynamicBVH8::FInsertion FDynamicBVH8::FindInsertion(const FBounds& LeafBounds) {
    const auto Compare = [](const FCandidate& A, const FCandidate& B) { return A.Lower != B.Lower ? A.Lower > B.Lower : A.Depth > B.Depth; };
    mCandidates.clear();
    FInsertion Best{}; Best.Cost = std::numeric_limits<double>::infinity();
    const auto Push = [&](Uint32 Ref, FParent Parent, const FBounds& Bounds, double Inherited, Uint32 Depth) {
        const double CombinedArea = FBounds::Merge(Bounds, LeafBounds).Area();
        const double Lower = Inherited + ((Ref & BVH8::LeafBit) ? CombinedArea : (std::max)(0.0, CombinedArea - Bounds.Area()));
        if (Lower > Best.Cost || (Best.Append && Lower >= Best.Cost)) return;
        mCandidates.push_back({Ref, Parent, Bounds, Inherited, CombinedArea, Lower, Depth}); std::push_heap(mCandidates.begin(), mCandidates.end(), Compare);
    };
    Push(mRootReference, {}, mBounds[mRootReference & BVH8::IndexMask], 0.0, 0);
    Best.Candidate = mCandidates.front();
    Uint32 BestHeight = UINT32_MAX, BestSubtreeHeight = UINT32_MAX;
    const Uint32 RootHeight = mHeights[mRootReference & BVH8::IndexMask];
    while (!mCandidates.empty()) {
        const auto Candidate = mCandidates.front();
        if (Candidate.Lower > Best.Cost || (Best.Append && Candidate.Lower >= Best.Cost)) break;
        std::pop_heap(mCandidates.begin(), mCandidates.end(), Compare); mCandidates.pop_back();
        const Uint32 Index = Candidate.Ref & BVH8::IndexMask;
        const bool Leaf = (Candidate.Ref & BVH8::LeafBit) != 0;
        const double ParentCost = Candidate.Inherited + Candidate.CombinedArea;
        const Uint32 SubtreeHeight = Leaf ? 0 : mHeights[Index], Height = (std::max)(RootHeight, Candidate.Depth + SubtreeHeight + 1);
        if (ParentCost < Best.Cost || (ParentCost == Best.Cost && !Best.Append && (Height < BestHeight || (Height == BestHeight && SubtreeHeight < BestSubtreeHeight)))) {
            Best = {Candidate, ParentCost, false}; BestHeight = Height; BestSubtreeHeight = SubtreeHeight;
        }
        if (Leaf) continue;
        const double DescendCost = Candidate.Lower;
        if (mCounts[Index] < 8 && DescendCost <= Best.Cost) { Best = {Candidate, DescendCost, true}; continue; }
        if (DescendCost > Best.Cost) continue;
        for (Uint32 Lane = 0; Lane < mCounts[Index]; ++Lane) Push(mNodes[Index].Children[Lane], {Index, Lane}, ReadBounds(Index, Lane), DescendCost, Candidate.Depth + 1);
    }
    mCandidates.clear();
    return Best;
}

void FDynamicBVH8::Insert(Uint32 Leaf, const DirectX::BoundingBox& Box) {
    const Uint32 LeafRef = BVH8::MakeReference(Leaf, 1, true);
    const auto LeafBounds = FBounds::FromBox(Box);
    if (Leaf < mLeafParents.size() && mLeafParents[Leaf].Node != Invalid) throw std::invalid_argument("Duplicate BVH leaf");
    if (mNodes.empty()) {
        const Uint32 Root = AllocateNode(); mCounts[Root] = 1;
        WriteChild(Root, 0, LeafRef, LeafBounds); Refresh(Root); return;
    }
    const auto Insertion = FindInsertion(LeafBounds); const auto& Best = Insertion.Candidate;
    if (Insertion.Append) {
        const Uint32 Index = Best.Ref & BVH8::IndexMask, Lane = mCounts[Index]++;
        WriteChild(Index, Lane, LeafRef, LeafBounds); RefitAncestors(Index);
    } else {
        const Uint32 Index = AllocateNode(); mCounts[Index] = 2;
        WriteChild(Index, 0, Best.Ref, Best.Bounds); WriteChild(Index, 1, LeafRef, LeafBounds);
        if (Best.Parent.Node != Invalid) WriteChild(Best.Parent.Node, Best.Parent.Lane, Reference(Index), FBounds::Merge(Best.Bounds, LeafBounds));
        RefitAncestors(Index);
    }
}

void FDynamicBVH8::Remove(Uint32 Leaf) {
    if (Leaf >= mLeafParents.size() || mLeafParents[Leaf].Node == Invalid) return;
    const auto Parent = mLeafParents[Leaf]; mLeafParents[Leaf] = {};
    const Uint32 Index = Parent.Node, Last = --mCounts[Index];
    if (Parent.Lane != Last) WriteChild(Index, Parent.Lane, mNodes[Index].Children[Last], ReadBounds(Index, Last));
    auto& Node = mNodes[Index];
    Node.MinX[Last] = Node.MinY[Last] = Node.MinZ[Last] = Node.MaxX[Last] = Node.MaxY[Last] = Node.MaxZ[Last] = 0; Node.Children[Last] = 0;
    if (Last == 0) { Clear(); return; }
    if (Last == 1) {
        const auto Grandparent = mParents[Index]; const Uint32 Child = Node.Children[0]; const auto Bounds = ReadBounds(Index, 0);
        if (Grandparent.Node != Invalid) {
            WriteChild(Grandparent.Node, Grandparent.Lane, Child, Bounds); FreeNode(Index); RefitAncestors(Grandparent.Node); return;
        }
        if (!(Child & BVH8::LeafBit)) {
            const Uint32 ChildIndex = Child & BVH8::IndexMask;
            mParents[ChildIndex] = {}; mRootReference = Child; FreeNode(Index); return;
        }
    }
    RefitAncestors(Index);
}

void FDynamicBVH8::Update(Uint32 Leaf, const DirectX::BoundingBox& Box) {
    const auto Bounds = FBounds::FromBox(Box);
    if (Leaf < mLeafParents.size() && mLeafParents[Leaf].Node != Invalid) {
        const auto Parent = mLeafParents[Leaf];
        if (ReadBounds(Parent.Node, Parent.Lane) == Bounds) return;
        Remove(Leaf);
    }
    Insert(Leaf, Box);
}
