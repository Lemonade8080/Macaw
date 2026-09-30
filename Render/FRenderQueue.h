#pragma once

#include "FRenderView.h"
#include "FRenderScene.h"

class IAssetRegistry;

struct FMeshDrawRecord {
    Uint32 mObjectIndex{};
    Uint32 mMaterialIndex{};
    Uint32 mFlags{};
    Uint32 mPadding{};
};

static_assert(sizeof(FMeshDrawRecord) == 16);

struct FMeshDrawBatch {
    FMeshDrawState mState{};

    Uint32 mFirstRecord{};
    Uint32 mRecordCount{};
    Uint32 mFlags{};
};

class FRenderQueue {
private:
    struct FVisibleObject {
        Uint32 mObjectIndex{};
        Uint32 mLODLevel{};
        Uint32 mFlags{};
    };

    struct FSceneCacheKey {
        const FRenderScene* mScene{nullptr};
        Uint64 mSceneId{};
        Uint64 mObjectRevision{};
        Uint64 mTemplateRevision{};
        CameraProbe mCamera{};
        FObjectHandle mSelectedActorHandle{};
        bool mUseLOD{};
        bool mRenderSky{};
        bool mSceneGeometry{};
        bool mSelectionOutline{};
    };

public:
    void Build(const IAssetRegistry* Registry, const FRenderScene& Scene, const FRenderView& View);

    const TArray<FMeshDrawBatch>& GetItems(ERenderPass Pass) const;
    const TArray<FMeshDrawRecord>& GetDrawRecords() const;
    const TArray<FMatrix>& GetGizmoTransforms() const;

private:
    bool IsSceneCacheCurrent(const FRenderScene& Scene, const FRenderView& View) const;
    void CommitSceneCache(const FRenderScene& Scene, const FRenderView& View);

    void BuildSceneItems(const FRenderScene& Scene, const FRenderView& View);
    void BuildGizmoItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes);

    Uint32 SelectLODLevel(const FRenderSceneObject& Object, const FRenderTemplateGroup& Group, const CameraProbe& Camera, float ProjectionScale, bool Perspective) const;

private:
    FSceneCacheKey mSceneCacheKey{};
    std::size_t mSceneRecordCount{};

    TArray<FMeshDrawBatch> mSceneItems{};
    TArray<FMeshDrawBatch> mOutlineItems{};
    TArray<FMeshDrawBatch> mGizmoItems{};
    TArray<FMeshDrawBatch> mEmptyItems{};

    TArray<FMeshDrawRecord> mDrawRecords{};

    TArray<Uint32> mVisibleObjectIndices{};
    TArray<Uint32> mBoundaryObjectPositions{};
    TArray<FVisibleObject> mVisibleObjects{};

    TArray<Uint32> mBucketCounts{};
    TArray<Uint32> mBucketWritePositions{};

    TArray<FMatrix> mGizmoTransforms{};
    TArray<FRenderBatchTemplate> mGizmoTemplates{};
};
