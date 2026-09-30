#include "pch.h"
#include "FBVHTree.h"

void FBVHTree::Build(const TArray<FActorProbe>& Probes)
{
	Clear();

	if (Probes.empty())
	{
		return;
	}

	TArray<Int32> Indices(Probes.size());
	for (size_t i = 0; i < Probes.size(); ++i)
	{
		Indices[i] = i;
	}

	mNodes.reserve(Probes.size() * 2);

	mRootIndex = BuildRecursive(Probes, Indices, 0, Indices.size());

}

void FBVHTree::Clear()
{
	mNodes.clear();
	mRootIndex = -1;
}

void FBVHTree::FrustumCull(const FFrustum& Frustum, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const
{
	if (mRootIndex == -1 || Probes.empty())
	{
		return;
	}

	CullRecursive(mRootIndex, Frustum, Probes, OutVisibleProbes);
}

Int32 FBVHTree::BuildRecursive(const TArray<FActorProbe>& Probes, TArray<Int32>& Indices, size_t Start, size_t End)
{
	const size_t Count = End - Start;
	if (Count == 0)
	{
		return -1;
	}

	const Int32 NodeIndex = static_cast<Int32>(mNodes.size());
	mNodes.emplace_back();

	if (1 == Count)
	{
		mNodes[NodeIndex].mBounds = Probes[Indices[Start]].mWorldAABB; // 리프 노드 바운드 세팅
		mNodes[NodeIndex].mProbeIndex = Indices[Start];
		mNodes[NodeIndex].mLeftChild = -1;
		mNodes[NodeIndex].mRightChild = -1;
		return NodeIndex;
	}

	DirectX::XMFLOAT3 MinCenter{ FLT_MAX, FLT_MAX, FLT_MAX };
	DirectX::XMFLOAT3 MaxCenter{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (size_t i = Start; i < End; ++i)
	{
		const auto& C = Probes[Indices[i]].mWorldAABB.Center;
		MinCenter.x = std::min(MinCenter.x, C.x);
		MinCenter.y = std::min(MinCenter.y, C.y);
		MinCenter.z = std::min(MinCenter.z, C.z);
		MaxCenter.x = std::max(MaxCenter.x, C.x);
		MaxCenter.y = std::max(MaxCenter.y, C.y);
		MaxCenter.z = std::max(MaxCenter.z, C.z);
	}
	int Axis = 0;
	const float ExtentX = MaxCenter.x - MinCenter.x;
	const float ExtentY = MaxCenter.y - MinCenter.y;
	const float ExtentZ = MaxCenter.z - MinCenter.z;
	if (ExtentY > ExtentX && ExtentY > ExtentZ) {
		Axis = 1;
	}
	else if (ExtentZ > ExtentX && ExtentZ > ExtentY) {
		Axis = 2;
	}

	const size_t Mid = Start + Count / 2;
	std::nth_element(Indices.begin() + Start, Indices.begin() + Mid, Indices.begin() + End,
		[&](Int32 A, Int32 B) {
			const auto& CenterA = Probes[A].mWorldAABB.Center;
			const auto& CenterB = Probes[B].mWorldAABB.Center;
			if (0 == Axis) return CenterA.x < CenterB.x;
			if (1 == Axis) return CenterA.y < CenterB.y;
			return CenterA.z < CenterB.z;
		});


	const Int32 Left = BuildRecursive(Probes, Indices, Start, Mid);
	const Int32 Right = BuildRecursive(Probes, Indices, Mid, End);

	mNodes[NodeIndex].mLeftChild = Left;
	mNodes[NodeIndex].mRightChild = Right;
	mNodes[NodeIndex].mProbeIndex = -1;

	DirectX::BoundingBox::CreateMerged(mNodes[NodeIndex].mBounds, mNodes[Left].mBounds, mNodes[Right].mBounds);
	return NodeIndex;
}

void FBVHTree::CullRecursive(Int32 NodeIndex, const FFrustum& Frustum, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const
{
	if (NodeIndex == -1) return;
	const FBVHNode& Node = mNodes[NodeIndex];

	const DirectX::ContainmentType Containment = Frustum.Contains(Node.mBounds);

	if (DirectX::DISJOINT == Containment)
	{
		return;
	}

	if (DirectX::CONTAINS == Containment)
	{
		CollectAllLeaves(NodeIndex, Probes, OutVisibleProbes);
		return;
	}

	if (Node.IsLeaf())
	{
		const auto& Probe = Probes[Node.mProbeIndex];
		if (Frustum.Intersects(Probe.mWorldOBB))
		{
			OutVisibleProbes.push_back(Probe);
		}
	}
	else
	{
		CullRecursive(Node.mLeftChild, Frustum, Probes, OutVisibleProbes);
		CullRecursive(Node.mRightChild, Frustum, Probes, OutVisibleProbes);
	}
}

void FBVHTree::CollectAllLeaves(Int32 NodeIndex, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const
{
	if (-1 == NodeIndex) return;
	const FBVHNode& Node = mNodes[NodeIndex];

	if (Node.IsLeaf())
	{
		OutVisibleProbes.push_back(Probes[Node.mProbeIndex]);
		return;
	}

	CollectAllLeaves(Node.mLeftChild, Probes, OutVisibleProbes);
	CollectAllLeaves(Node.mRightChild, Probes, OutVisibleProbes);
}
