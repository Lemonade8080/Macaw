#include "pch.h"
#include "FRenderScene.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"
#include "Asset/Pipeline/UPipeline.h"

#include <algorithm>
#include <cstring>
#include <tuple>

namespace {
    constexpr std::size_t MaximumJournalChanges{262144};

    Uint64 MakeObjectKey(FObjectHandle Handle) {
        return (static_cast<Uint64>(Handle.mGeneration) << 32) | Handle.mIndex;
    }

    bool HasUsableBounds(const DirectX::BoundingBox& Bounds) {
        return std::isfinite(Bounds.Center.x) && std::isfinite(Bounds.Center.y) && std::isfinite(Bounds.Center.z) && std::isfinite(Bounds.Extents.x) && std::isfinite(Bounds.Extents.y) && std::isfinite(Bounds.Extents.z) && Bounds.Extents.x >= 0.0f && Bounds.Extents.y >= 0.0f && Bounds.Extents.z >= 0.0f && (Bounds.Extents.x > 0.0f || Bounds.Extents.y > 0.0f || Bounds.Extents.z > 0.0f);
    }
}

bool operator<(const FRenderTemplateGroupKey& Left, const FRenderTemplateGroupKey& Right) {
    return std::tie(Left.mPipelineHandle.mId, Left.mPipelineHandle.mGeneration, Left.mMaterialHandle.mId, Left.mMaterialHandle.mGeneration, Left.mMeshHandle.mId, Left.mMeshHandle.mGeneration) < std::tie(Right.mPipelineHandle.mId, Right.mPipelineHandle.mGeneration, Right.mMaterialHandle.mId, Right.mMaterialHandle.mGeneration, Right.mMeshHandle.mId, Right.mMeshHandle.mGeneration);
}

FRenderScene::FRenderScene(Uint64 SceneId)
	: mSceneId{SceneId} {
}

void FRenderScene::Synchronize(const IAssetRegistry* Registry, FSceneRenderData& Scene) {
    mLightProbes.swap(Scene.mLightProbes);
    mTextProbes.swap(Scene.mTextProbes);
    mBillboardProbes.swap(Scene.mBillboardProbes);

    Scene.mLightProbes.clear();
    Scene.mTextProbes.clear();
    Scene.mBillboardProbes.clear();

    mChangedObjects.clear();
    mObjectsChanged = false;

    if (!mSourceRevision.IsCurrent(Scene.mRevision)) {
        ApplyObjectUpdates(Scene);
        mSourceRevision.Commit(Scene.mRevision);
    }

    CommitObjectChanges();

    UpdateBounds();

    RefreshTemplates(Registry);
}

Uint64 FRenderScene::GetId() const {
    return mSceneId;
}

Uint64 FRenderScene::GetRevision() const {
    return mRevision;
}

const TArray<FMatrix>& FRenderScene::GetObjectTransforms() const {
    return mObjectTransforms;
}

ERenderUpdateMode FRenderScene::CollectChangedObjects(Uint64 SinceRevision, TArray<Uint32>& OutIndices) const {
    OutIndices.clear();

    if (SinceRevision == mRevision) {
        return ERenderUpdateMode::Partial;
    }

    if (SinceRevision == 0 || SinceRevision > mRevision || SinceRevision < mJournalFloor) {
        return ERenderUpdateMode::Full;
    }

    for (auto Position{mChanges.rbegin()}; Position != mChanges.rend() && Position->mRevision > SinceRevision; ++Position) {
        OutIndices.push_back(Position->mObjectIndex);
    }

    std::sort(OutIndices.begin(), OutIndices.end());
    OutIndices.erase(std::unique(OutIndices.begin(), OutIndices.end()), OutIndices.end());
    return ERenderUpdateMode::Partial;
}

const TArray<FRenderSceneObject>& FRenderScene::GetObjects() const {
    return mObjects;
}

const TArray<FRenderBatchTemplate>& FRenderScene::GetTemplates() const {
    return mTemplates;
}

const TArray<FRenderTemplateGroup>& FRenderScene::GetTemplateGroups() const {
    return mTemplateGroups;
}

const TArray<FLightProbe>& FRenderScene::GetLightProbes() const {
    return mLightProbes;
}

const TArray<FTextProbe>& FRenderScene::GetTextProbes() const {
    return mTextProbes;
}

const TArray<FBillboardProbe>& FRenderScene::GetBillboardProbes() const {
    return mBillboardProbes;
}

void FRenderScene::CollectVisibleObjects(const FFrustum& Frustum, TArray<Uint32>& OutIndices) const {
    OutIndices.reserve(mBoundsObjects.size() + mUnboundedObjects.size());
    mBoundsTree.FrustumCull(Frustum, OutIndices);

    std::erase_if(OutIndices, [this, &Frustum](Uint32 ObjectIndex) {
        return !Frustum.Intersects(mObjects[ObjectIndex].mWorldOBB);
    });

    OutIndices.insert(OutIndices.end(), mUnboundedObjects.begin(), mUnboundedObjects.end());
}

void FRenderScene::CollectVisibleObjects(const FMatrix& ViewProjection, TArray<Uint32>& OutIndices) const {
    OutIndices.reserve(mBoundsObjects.size() + mUnboundedObjects.size());
    mBoundsTree.FrustumCull(ViewProjection, OutIndices);

    OutIndices.insert(OutIndices.end(), mUnboundedObjects.begin(), mUnboundedObjects.end());
}

void FRenderScene::ApplyObjectUpdates(const FSceneRenderData& Scene) {
    for (const FRenderObjectUpdate& Update : Scene.mObjectUpdates) {
        if (!Update.mComponentHandle.IsValid()) {
            continue;
        }

        const Uint64 Key{MakeObjectKey(Update.mComponentHandle)};
        const auto Position{mObjectLookup.find(Key)};

        if (Update.mRemoved) {
            if (Position != mObjectLookup.end()) {
                RemoveObject(Position->second);
                mObjectLookup.erase(Position);
            }
        } else if (Position != mObjectLookup.end()) {
            UpdateObject(Position->second, Update.mProbe);
        } else {
            const Uint32 ObjectIndex{AddObject(Update.mComponentHandle, Update.mProbe)};
            if (ObjectIndex != UINT32_MAX) {
                mObjectLookup.emplace(Key, ObjectIndex);
            }
        }
    }
}

Uint32 FRenderScene::FindOrAddTemplateGroup(const FActorProbe& Probe) {
    const FRenderTemplateGroupKey Key{Probe.mPipelineHandle, Probe.mMaterialHandle, Probe.mMeshHandle};
    const auto Position{mTemplateGroupIndicesByKey.find(Key)};
    if (Position != mTemplateGroupIndicesByKey.end()) {
        return Position->second;
    }

    const Uint32 GroupIndex{static_cast<Uint32>(mTemplateGroups.size())};
    FRenderTemplateGroup Group{};
    Group.mKey = Key;

    mTemplateGroups.push_back(Group);
    mTemplateGroupIndicesByKey.emplace(Key, GroupIndex);
    mTemplatesDirty = true;

    return GroupIndex;
}

Uint32 FRenderScene::AddObject(FObjectHandle ComponentHandle, const FActorProbe& Probe) {
    Uint32 ObjectIndex{};
    if (mFreeObjects.empty()) {
        if (mObjects.size() >= 0x80000000ull) {
            return UINT32_MAX;
        }

        ObjectIndex = static_cast<Uint32>(mObjects.size());
        mObjects.emplace_back();
        mObjectTransforms.emplace_back();
    } else {
        ObjectIndex = mFreeObjects.back();
        mFreeObjects.pop_back();
    }

    FRenderSceneObject& Object{mObjects[ObjectIndex]};
    Object = {};
    Object.mComponentHandle = ComponentHandle;
    Object.mActive = true;
    mTopologyDirty = true;

    UpdateObject(ObjectIndex, Probe);

    return ObjectIndex;
}

void FRenderScene::UpdateObject(Uint32 ObjectIndex, const FActorProbe& Probe) {
    FRenderSceneObject& Object{mObjects[ObjectIndex]};
    Uint32 GroupIndex{Object.mTemplateGroupIndex};
    if (GroupIndex == UINT32_MAX || mTemplateGroups[GroupIndex].mKey.mPipelineHandle != Probe.mPipelineHandle || mTemplateGroups[GroupIndex].mKey.mMaterialHandle != Probe.mMaterialHandle || mTemplateGroups[GroupIndex].mKey.mMeshHandle != Probe.mMeshHandle) {
        GroupIndex = FindOrAddTemplateGroup(Probe);
    }

    const bool Cullable{HasUsableBounds(Probe.mWorldAABB)};
    const bool TransformChanged{std::memcmp(&mObjectTransforms[ObjectIndex], &Probe.mWorld, sizeof(FMatrix)) != 0};
    const bool BoundsChanged{std::memcmp(&Object.mWorldSphereBounds, &Probe.mWorldSphereBounds, sizeof(DirectX::BoundingSphere)) != 0 || std::memcmp(&Object.mWorldOBB, &Probe.mWorldOBB, sizeof(DirectX::BoundingOrientedBox)) != 0 || std::memcmp(&Object.mWorldAABB, &Probe.mWorldAABB, sizeof(DirectX::BoundingBox)) != 0};
    if (Object.mTemplateGroupIndex == GroupIndex && Object.mOwnerHandle == Probe.mOwnerHandle && Object.mFlags == Probe.mFlags && !TransformChanged && !BoundsChanged) {
        return;
    }

    if (Object.mTemplateGroupIndex != GroupIndex) {
        if (Object.mTemplateGroupIndex != UINT32_MAX) {
            FRenderTemplateGroup& PreviousGroup{mTemplateGroups[Object.mTemplateGroupIndex]};
            --PreviousGroup.mReferenceCount;
            mTemplatesDirty = mTemplatesDirty || PreviousGroup.mReferenceCount == 0;
        }

        mTemplatesDirty = mTemplatesDirty || mTemplateGroups[GroupIndex].mReferenceCount == 0;
        ++mTemplateGroups[GroupIndex].mReferenceCount;
        Object.mTemplateGroupIndex = GroupIndex;
    }

    if (Object.mCullable != Cullable) {
        mTopologyDirty = true;
    }

    Object.mOwnerHandle = Probe.mOwnerHandle;
    Object.mFlags = Probe.mFlags;
    Object.mWorldSphereBounds = Probe.mWorldSphereBounds;
    Object.mWorldOBB = Probe.mWorldOBB;
    Object.mWorldAABB = Probe.mWorldAABB;
    Object.mCullable = Cullable;
    mObjectTransforms[ObjectIndex] = Probe.mWorld;

    RecordObjectChange(ObjectIndex);
}

void FRenderScene::RemoveObject(Uint32 ObjectIndex) {
    FRenderSceneObject& Object{mObjects[ObjectIndex]};
    if (!Object.mActive) {
        return;
    }

    Object.mActive = false;
    if (Object.mTemplateGroupIndex != UINT32_MAX) {
        FRenderTemplateGroup& Group{mTemplateGroups[Object.mTemplateGroupIndex]};
        --Group.mReferenceCount;
        mTemplatesDirty = mTemplatesDirty || Group.mReferenceCount == 0;
    }

    mFreeObjects.push_back(ObjectIndex);
    mTopologyDirty = true;

    RecordObjectChange(ObjectIndex);
}

void FRenderScene::RecordObjectChange(Uint32 ObjectIndex) {
    mObjectsChanged = true;
    mChangedObjects.push_back(ObjectIndex);
}

void FRenderScene::CommitObjectChanges() {
    if (mObjectsChanged) {
        ++mRevision;
        std::sort(mChangedObjects.begin(), mChangedObjects.end());
        mChangedObjects.erase(std::unique(mChangedObjects.begin(), mChangedObjects.end()), mChangedObjects.end());

        for (const Uint32 ObjectIndex : mChangedObjects) {
            mChanges.push_back(FObjectChange{mRevision, ObjectIndex});
        }

        while (mChanges.size() > MaximumJournalChanges) {
            mJournalFloor = (std::max)(mJournalFloor, mChanges.front().mRevision);
            mChanges.pop_front();
        }
    }
}

void FRenderScene::RefreshTemplates(const IAssetRegistry* Registry) {
    const FAssetHandle SkyPipeline{Registry != nullptr ? Registry->FindAsset(FAssetPath{"/Game/Pipeline/SkyDome.json"}) : FAssetHandle{}};

    for (FRenderTemplateGroup& Group : mTemplateGroups) {
        Group.mSky = Group.mKey.mPipelineHandle == SkyPipeline;
        if (Group.mReferenceCount == 0) {
            continue;
        }

        const UMesh* Mesh{Registry != nullptr ? Registry->ResolveAsset<UMesh>(Group.mKey.mMeshHandle) : nullptr};
        const UMaterial* Material{Registry != nullptr ? Registry->ResolveAsset<UMaterial>(Group.mKey.mMaterialHandle) : nullptr};
        const UPipeline* Pipeline{Registry != nullptr ? Registry->ResolveAsset<UPipeline>(Group.mKey.mPipelineHandle) : nullptr};
        const Uint64 MeshRevision{Mesh != nullptr ? Mesh->GetRenderRevision() : 0};
        const Uint64 MaterialRevision{Material != nullptr ? Material->GetRenderRevision() : 0};

        const bool MeshChanged{Group.mMesh.Update(Mesh, MeshRevision)};
        const bool MaterialChanged{Group.mMaterial.Update(Material, MaterialRevision)};

        if (MeshChanged || MaterialChanged || Group.mPipeline != Pipeline) {
            Group.mPipeline = Pipeline;
            mTemplatesDirty = true;
        }
    }

    if (!mTemplatesDirty) {
        return;
    }

    mTemplates.clear();

    for (FRenderTemplateGroup& Group : mTemplateGroups) {
        Group.mTemplateRangesByLOD.fill(FRenderTemplateRange{});
        Group.mAvailableLODMask = 0;

        const UMesh* Mesh{Group.mMesh.GetValue()};
        const UMaterial* Material{Group.mMaterial.GetValue()};
        if (Group.mReferenceCount == 0 || Mesh == nullptr || Material == nullptr || Group.mPipeline == nullptr) {
            continue;
        }

        for (Uint32 Level{}; Level < GLODCount; ++Level) {
            if (Level != 0 && !Mesh->HasLOD(static_cast<int>(Level))) {
                continue;
            }

            const Uint32 FirstTemplateIndex{static_cast<Uint32>(mTemplates.size())};
            AppendMeshDrawTemplates(*Mesh, *Material, Group.mKey.mPipelineHandle, Group.mKey.mMeshHandle, Level, mTemplates);

            FRenderTemplateRange& TemplateRange{Group.mTemplateRangesByLOD[Level]};
            TemplateRange = FRenderTemplateRange{FirstTemplateIndex, static_cast<Uint32>(mTemplates.size()) - FirstTemplateIndex};
            if (TemplateRange.mTemplateCount != 0) {
                Group.mAvailableLODMask |= 1u << Level;
            }
        }
    }

    mTemplatesDirty = false;
}

void FRenderScene::UpdateBounds() {
    if (mTopologyDirty) {
        RebuildBounds();
    } else {
        for (const Uint32 ObjectIndex : mChangedObjects) {
            mBoundsTree.UpdateBounds(ObjectIndex, mObjects[ObjectIndex].mWorldAABB);
        }

        mBoundsTree.Refit(mChangedObjects);
    }
}

void FRenderScene::RebuildBounds() {
    mBuildBounds.resize(mObjects.size());
    mBoundsObjects.clear();
    mUnboundedObjects.clear();

    for (Uint32 Index{}; Index < mObjects.size(); ++Index) {
        if (!mObjects[Index].mActive) {
            continue;
        }

        if (mObjects[Index].mCullable) {
            mBuildBounds[Index] = mObjects[Index].mWorldAABB;
            mBoundsObjects.push_back(Index);
        } else {
            mUnboundedObjects.push_back(Index);
        }
    }

    mBoundsTree.Build(mBuildBounds, mBoundsObjects);
    mTopologyDirty = false;
}
