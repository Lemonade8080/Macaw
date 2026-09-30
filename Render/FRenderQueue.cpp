#include "pch.h"
#include "FRenderQueue.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"

#include <algorithm>

void FRenderQueue::Build(const IAssetRegistry* Registry, const FRenderScene& Scene, const FRenderView& View) {
    mSceneItems.clear();
    mOutlineItems.clear();
    mGizmoItems.clear();
    mGizmoTransforms.clear();

    if (View.IsPassEnabled(ERenderPass::SceneGeometry)) {
        BuildSceneItems(Scene, View);
    } else {
        mDrawRecords.clear();
    }

    if (View.IsPassEnabled(ERenderPass::SelectionOutline)) {
        for (const FMeshDrawBatch& Item : mSceneItems) {
            if ((Item.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0) {
                mOutlineItems.push_back(Item);
            }
        }
    }

    if (View.IsPassEnabled(ERenderPass::Gizmo)) {
        BuildGizmoItems(Registry, View.mGizmoProbes);
    }
}

const TArray<FMeshDrawBatch>& FRenderQueue::GetItems(ERenderPass Pass) const {
    switch (Pass) {
        case ERenderPass::SceneGeometry:
            return mSceneItems;
        case ERenderPass::SelectionOutline:
            return mOutlineItems;
        case ERenderPass::Gizmo:
            return mGizmoItems;
        default:
            return mEmptyItems;
    }
}

const TArray<FMeshDrawRecord>& FRenderQueue::GetDrawRecords() const {
    return mDrawRecords;
}

const TArray<FMatrix>& FRenderQueue::GetGizmoTransforms() const {
    return mGizmoTransforms;
}

void FRenderQueue::BuildSceneItems(const FRenderScene& Scene, const FRenderView& View) {
    const TArray<FRenderSceneObject>& Objects{Scene.GetObjects()};
    const TArray<FRenderTemplateGroup>& Groups{Scene.GetTemplateGroups()};
    const TArray<FRenderBatchTemplate>& Templates{Scene.GetTemplates()};
    if (Templates.empty()) {
        mDrawRecords.clear();
        return;
    }

    const bool Perspective{std::abs(View.mCamera.mProjection.M[2][3]) > 1e-6f};
    const float ProjectionScale{std::abs(View.mCamera.mProjection.M[1][1])};

    if (Perspective) {
        Scene.CollectVisibleObjects(View.mCamera.mViewFrustum, mVisibleObjectIndices);
    } else {
        Scene.CollectVisibleObjects(View.mCamera.mViewProjection, mVisibleObjectIndices);
    }

    mVisibleObjects.clear();
    mVisibleObjects.reserve(mVisibleObjectIndices.size());
    mBucketCounts.assign(Templates.size() * 2, 0);
    mBucketWritePositions.resize(mBucketCounts.size());

    constexpr Uint32 SelectedFlag{static_cast<Uint32>(ERenderObjectFlags::Selected)};
    for (const Uint32 ObjectIndex : mVisibleObjectIndices) {
        const FRenderSceneObject& Object{Objects[ObjectIndex]};
        const FRenderTemplateGroup& Group{Groups[Object.mTemplateGroupIndex]};
        if (!View.mSettings.mBRenderSky && Group.mSky) {
            continue;
        }

        const Uint32 LODLevel{Group.mSky || !View.mUseLOD ? 0 : SelectLODLevel(Object, Group, View.mCamera, ProjectionScale, Perspective)};
        const FRenderTemplateRange& TemplateRange{Group.mTemplateRangesByLOD[LODLevel]};
        if (TemplateRange.mTemplateCount == 0) {
            continue;
        }

        const bool Selected{View.mSelectedActorHandle.IsValid() && Object.mOwnerHandle == View.mSelectedActorHandle};
        const Uint32 Flags{Object.mFlags | (Selected ? SelectedFlag : 0)};
        const Uint32 BucketFlag{(Flags & SelectedFlag) != 0 ? 1u : 0u};
        mVisibleObjects.push_back(FVisibleObject{ObjectIndex, LODLevel, Flags});

        for (Uint32 Index{}; Index < TemplateRange.mTemplateCount; ++Index) {
            ++mBucketCounts[(TemplateRange.mFirstTemplateIndex + Index) * 2 + BucketFlag];
        }
    }

    std::size_t TotalRecords{};
    for (std::size_t Bucket{}; Bucket < mBucketCounts.size(); ++Bucket) {
        const Uint32 Count{mBucketCounts[Bucket]};
        if (Count == 0) {
            continue;
        }

        if (TotalRecords > UINT32_MAX - static_cast<std::size_t>(Count)) {
            mSceneItems.clear();
            mDrawRecords.clear();
            return;
        }

        const FRenderBatchTemplate& Template{Templates[Bucket / 2]};
        const Uint32 FirstRecord{static_cast<Uint32>(TotalRecords)};
        const Uint32 Flags{(Bucket & 1u) != 0 ? SelectedFlag : 0};
        mSceneItems.push_back(FMeshDrawBatch{Template.mState, FirstRecord, Count, Flags});
        mBucketWritePositions[Bucket] = FirstRecord;
        TotalRecords += Count;
    }

    mDrawRecords.resize(TotalRecords);
    for (const FVisibleObject& VisibleObject : mVisibleObjects) {
        const FRenderTemplateGroup& Group{Groups[Objects[VisibleObject.mObjectIndex].mTemplateGroupIndex]};
        const FRenderTemplateRange& TemplateRange{Group.mTemplateRangesByLOD[VisibleObject.mLODLevel]};
        const Uint32 BucketFlag{(VisibleObject.mFlags & SelectedFlag) != 0 ? 1u : 0u};

        for (Uint32 Index{}; Index < TemplateRange.mTemplateCount; ++Index) {
            const Uint32 TemplateIndex{TemplateRange.mFirstTemplateIndex + Index};
            const Uint32 Destination{mBucketWritePositions[TemplateIndex * 2 + BucketFlag]++};
            mDrawRecords[Destination] = FMeshDrawRecord{VisibleObject.mObjectIndex, Templates[TemplateIndex].mMaterialIndex, VisibleObject.mFlags, 0};
        }
    }
}

void FRenderQueue::BuildGizmoItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes) {
    if (Registry == nullptr) {
        return;
    }

    mGizmoTransforms.reserve(Probes.size());
    for (const FActorProbe& Probe : Probes) {
        const UMesh* Mesh{Registry->ResolveAsset<UMesh>(Probe.mMeshHandle)};
        const UMaterial* Material{Registry->ResolveAsset<UMaterial>(Probe.mMaterialHandle)};
        if (Mesh == nullptr || Material == nullptr) {
            continue;
        }

        mGizmoTemplates.clear();
        AppendMeshDrawTemplates(*Mesh, *Material, Probe.mPipelineHandle, Probe.mMeshHandle, 0, mGizmoTemplates);
        if (mGizmoTemplates.empty()) {
            continue;
        }

        if (mGizmoTransforms.size() >= 0x80000000ull || mGizmoTemplates.size() > UINT32_MAX - mDrawRecords.size()) {
            break;
        }

        const Uint32 ObjectIndex{0x80000000u | static_cast<Uint32>(mGizmoTransforms.size())};
        mGizmoTransforms.push_back(Probe.mWorld);

        for (const FRenderBatchTemplate& Template : mGizmoTemplates) {
            const Uint32 FirstRecord{static_cast<Uint32>(mDrawRecords.size())};
            mDrawRecords.push_back(FMeshDrawRecord{ObjectIndex, Template.mMaterialIndex, Probe.mFlags, 0});
            mGizmoItems.push_back(FMeshDrawBatch{Template.mState, FirstRecord, 1, Probe.mFlags});
        }
    }
}

Uint32 FRenderQueue::SelectLODLevel(const FRenderSceneObject& Object, const FRenderTemplateGroup& Group, const CameraProbe& Camera, float ProjectionScale, bool Perspective) const {
    // LOD0만 있는 메시에는 화면 크기 계산이 필요 없다.
    if ((Group.mAvailableLODMask & ~1u) == 0) {
        return 0;
    }

    const DirectX::BoundingSphere& Bounds{Object.mWorldSphereBounds};
    if (Bounds.Radius <= 1e-4f || !std::isfinite(Bounds.Radius)) {
        return 0;
    }

    float ScreenSize{Bounds.Radius * ProjectionScale};
    if (Perspective) {
        // LOD 선택에 쓰는 카메라 깊이만 계산한다.
        const FMatrix& View{Camera.mView};
        const float ViewDepth{Bounds.Center.x * View.M[0][2] + Bounds.Center.y * View.M[1][2] +
            Bounds.Center.z * View.M[2][2] + View.M[3][2]};
        ScreenSize /= (std::max)(std::abs(ViewDepth), 1e-4f);
    }

    Uint32 Level{GLODCount - 1};
    for (Uint32 Index{}; Index < GLODCount; ++Index) {
        if (ScreenSize >= GLODSettings[Index].mScreenSize) {
            Level = Index;
            break;
        }
    }

    while (Level > 0 && (Group.mAvailableLODMask & (1u << Level)) == 0) {
        --Level;
    }

    return Level;
}
