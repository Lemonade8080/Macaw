#pragma once
#include <DirectXCollision.h>
#include "Core/Base/FRenderProbe.h"

struct FBVHNode
{
	DirectX::BoundingBox mBounds{};
	Int32 mLeftChild{ -1 };
	Int32 mRightChild{ -1 };
	Int32 mProbeIndex{ -1 };

	bool IsLeaf() const
	{
		return -1 == mLeftChild && -1 == mRightChild;
	}
};

class FBVHTree
{
public:
	FBVHTree() = default;

	void Build(const TArray<FActorProbe>& Probes);
	void Clear();
	const TArray<FBVHNode>& GetNodes() const { return mNodes; }

	void FrustumCull(const FFrustum& Frustum, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const;



private:
	Int32 BuildRecursive(const TArray<FActorProbe>& Probes, TArray<Int32>& Indices, size_t Start, size_t End);

	void CullRecursive(Int32 NodeIndex, const FFrustum& Frustum, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const;
	void CollectAllLeaves(Int32 NodeIndex, const TArray<FActorProbe>& Probes, TArray<FActorProbe>& OutVisibleProbes) const;
		 
private:
	TArray<FBVHNode> mNodes;
	Int32 mRootIndex{ -1 };
};

