#pragma once

#include "Core/Base/FRenderProbe.h"
#include "Core/Base/FRevisionCursor.h"
#include "Core/Base/TRevisioned.h"
#include "Asset/FLODSettings.h"
#include "FBVHTree.h"
#include "FMeshDrawState.h"

#include <array>
#include <deque>
#include <map>
#include <unordered_map>

class IAssetRegistry;
class UMaterial;
class UMesh;
class UPipeline;

enum class ERenderUpdateMode : Uint8 {
    Partial,
    Full
};

struct FRenderSceneObject {
    FObjectHandle mComponentHandle{};
    FObjectHandle mOwnerHandle{};

    DirectX::BoundingSphere mWorldSphereBounds{};
    DirectX::BoundingOrientedBox mWorldOBB{};
    DirectX::BoundingBox mWorldAABB{};

    Uint32 mTemplateGroupIndex{UINT32_MAX};
    Uint32 mFlags{};
    bool mActive{};
    bool mCullable{};
};

struct FRenderTemplateGroupKey {
    FAssetHandle mPipelineHandle{};
    FAssetHandle mMaterialHandle{};
    FAssetHandle mMeshHandle{};
};

bool operator<(const FRenderTemplateGroupKey& Left, const FRenderTemplateGroupKey& Right);

struct FRenderTemplateRange {
    Uint32 mFirstTemplateIndex{};
    Uint32 mTemplateCount{};
};

struct FRenderTemplateGroup {
    FRenderTemplateGroupKey mKey{};

    const UPipeline* mPipeline{};
    TRevisioned<const UMaterial*> mMaterial{};
    TRevisioned<const UMesh*> mMesh{};

    std::array<FRenderTemplateRange, GLODCount> mTemplateRangesByLOD{};
    Uint32 mAvailableLODMask{};

    Uint32 mReferenceCount{};
    bool mSky{};
};

class FRenderScene {
private:
    struct FObjectChange {
        Uint64 mRevision{};
        Uint32 mObjectIndex{};
    };

public:
    explicit FRenderScene(Uint64 SceneId);

public:
    void Synchronize(const IAssetRegistry* Registry, FSceneRenderData& Scene);

    Uint64 GetId() const;
    Uint64 GetRevision() const;
    const TArray<FMatrix>& GetObjectTransforms() const;

    ERenderUpdateMode CollectChangedObjects(Uint64 SinceRevision, TArray<Uint32>& OutIndices) const;

    const TArray<FRenderSceneObject>& GetObjects() const;
    const TArray<FRenderBatchTemplate>& GetTemplates() const;
    const TArray<FRenderTemplateGroup>& GetTemplateGroups() const;

    const TArray<FLightProbe>& GetLightProbes() const;
    const TArray<FTextProbe>& GetTextProbes() const;
    const TArray<FBillboardProbe>& GetBillboardProbes() const;

    void CollectVisibleObjects(const FFrustum& Frustum, TArray<Uint32>& OutIndices) const;
    void CollectVisibleObjects(const FMatrix& ViewProjection, TArray<Uint32>& OutIndices) const;

private:
    void ApplyObjectUpdates(const FSceneRenderData& Scene);

    Uint32 FindOrAddTemplateGroup(const FActorProbe& Probe);
    Uint32 AddObject(FObjectHandle ComponentHandle, const FActorProbe& Probe);
    void UpdateObject(Uint32 ObjectIndex, const FActorProbe& Probe);
    void RemoveObject(Uint32 ObjectIndex);

    void RecordObjectChange(Uint32 ObjectIndex);
    void CommitObjectChanges();

    void RefreshTemplates(const IAssetRegistry* Registry);
    void UpdateBounds();
    void RebuildBounds();

private:
    Uint64 mSceneId{};
    Uint64 mRevision{1};
    FRevisionCursor mSourceRevision{};
    Uint64 mJournalFloor{};

    bool mTopologyDirty{};
    bool mObjectsChanged{};
    bool mTemplatesDirty{true};

    TArray<FRenderSceneObject> mObjects{};
    TArray<FMatrix> mObjectTransforms{};
    TArray<Uint32> mFreeObjects{};

    TArray<FLightProbe> mLightProbes{};
    TArray<FTextProbe> mTextProbes{};
    TArray<FBillboardProbe> mBillboardProbes{};

    std::unordered_map<Uint64, Uint32> mObjectLookup{};

    std::map<FRenderTemplateGroupKey, Uint32> mTemplateGroupIndicesByKey{};
    TArray<FRenderTemplateGroup> mTemplateGroups{};
    TArray<FRenderBatchTemplate> mTemplates{};

    FBVHTree mBoundsTree{};
    TArray<DirectX::BoundingBox> mBuildBounds{};
    TArray<Uint32> mBoundsObjects{};
    TArray<Uint32> mUnboundedObjects{};

    TArray<Uint32> mChangedObjects{};
    std::deque<FObjectChange> mChanges{};
};
