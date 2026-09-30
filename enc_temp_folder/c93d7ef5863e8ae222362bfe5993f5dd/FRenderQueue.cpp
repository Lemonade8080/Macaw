#include "pch.h"
#include "FRenderQueue.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"
#include "FLODSelection.h"

#include <algorithm>
#include <cstring>

namespace {
    bool IsSameMatrix(const FMatrix& Left, const FMatrix& Right) {
        return std::memcmp(Left.M, Right.M, sizeof(Left.M)) == 0;
    }

    bool IsSameFrustum(const FFrustum& Left, const FFrustum& Right) {
        return Left.Origin.x == Right.Origin.x && Left.Origin.y == Right.Origin.y && Left.Origin.z == Right.Origin.z && Left.Orientation.x == Right.Orientation.x && Left.Orientation.y == Right.Orientation.y && Left.Orientation.z == Right.Orientation.z && Left.Orientation.w == Right.Orientation.w && Left.RightSlope == Right.RightSlope && Left.LeftSlope == Right.LeftSlope && Left.TopSlope == Right.TopSlope && Left.BottomSlope == Right.BottomSlope && Left.Near == Right.Near && Left.Far == Right.Far;
    }
}

void FRenderQueue::Build(const IAssetRegistry* Registry, const FRenderScene& Scene, const FRenderView& View) {
    mGizmoItems.clear();
    mGizmoTransforms.clear();

    if (!IsSceneCacheCurrent(Scene, View)) {
        mSceneItems.clear();
        mOutlineItems.clear();

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

        mSceneRecordCount = mDrawRecords.size();
        CommitSceneCache(Scene, View);
    }

    mDrawRecords.resize(mSceneRecordCount);
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

bool FRenderQueue::IsSceneCacheCurrent(const FRenderScene& Scene, const FRenderView& View) const {
    const float ViewportHeight{View.mTarget != nullptr ? View.mTarget->GetViewport().Height : 0.0f};
    if (mSceneCacheKey.mViewportHeight != ViewportHeight) { return false; }
    return mSceneCacheKey.mScene == &Scene && mSceneCacheKey.mSceneId == Scene.GetId() && mSceneCacheKey.mObjectRevision == Scene.GetRevision() && mSceneCacheKey.mTemplateRevision == Scene.GetTemplateRevision() && IsSameMatrix(mSceneCacheKey.mCamera.mView, View.mCamera.mView) && IsSameMatrix(mSceneCacheKey.mCamera.mProjection, View.mCamera.mProjection) && IsSameMatrix(mSceneCacheKey.mCamera.mViewProjection, View.mCamera.mViewProjection) && IsSameFrustum(mSceneCacheKey.mCamera.mViewFrustum, View.mCamera.mViewFrustum) && mSceneCacheKey.mSelectedActorHandle == View.mSelectedActorHandle && mSceneCacheKey.mUseLOD == View.mUseLOD && mSceneCacheKey.mRenderSky == View.mSettings.mBRenderSky && mSceneCacheKey.mSceneGeometry == View.IsPassEnabled(ERenderPass::SceneGeometry) && mSceneCacheKey.mSelectionOutline == View.IsPassEnabled(ERenderPass::SelectionOutline);
}

void FRenderQueue::CommitSceneCache(const FRenderScene& Scene, const FRenderView& View) {
    const float ViewportHeight{View.mTarget != nullptr ? View.mTarget->GetViewport().Height : 0.0f};
    mSceneCacheKey = FSceneCacheKey{&Scene, Scene.GetId(), Scene.GetRevision(), Scene.GetTemplateRevision(), View.mCamera, ViewportHeight, View.mSelectedActorHandle, View.mUseLOD, View.mSettings.mBRenderSky, View.IsPassEnabled(ERenderPass::SceneGeometry), View.IsPassEnabled(ERenderPass::SelectionOutline)};
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
    const float ViewportHeight{View.mTarget != nullptr ? View.mTarget->GetViewport().Height : 0.0f};

    Scene.CollectVisibleObjects(View.mCamera, mVisibleObjectIndices, mBoundaryObjectPositions);

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

        const bool Selected{View.mSelectedActorHandle.IsValid() && Object.mOwnerHandle == View.mSelectedActorHandle};
        const Uint32 Flags{Object.mFlags | (Selected ? SelectedFlag : 0)};
        const Uint32 BucketFlag{(Flags & SelectedFlag) != 0 ? 1u : 0u};
        FLODSelection LOD{};
        if (!Group.mSky && View.mUseLOD) {
            LOD = SelectMeshLOD(CalculateScreenSize(Object, View.mCamera, ProjectionScale, Perspective),
                ViewportHeight, Group.mAvailableLODMask, Object.mCullable && BucketFlag == 0);
        }
        if (LOD.mCulled) { continue; }

        auto AddLevel = [&](Uint32 Level, float Dither) {
            const FRenderTemplateRange& Range{Group.mTemplateRangesByLOD[Level]};
            if (Range.mTemplateCount == 0) { return; }
            mVisibleObjects.push_back(FVisibleObject{ObjectIndex, Level, Flags, Dither});
            for (Uint32 Index{}; Index < Range.mTemplateCount; ++Index) {
                ++mBucketCounts[(Range.mFirstTemplateIndex + Index) * 2 + BucketFlag];
            }
        };
        AddLevel(LOD.mLevel, LOD.mDither);
        if (LOD.mNextLevel != UINT32_MAX) { AddLevel(LOD.mNextLevel, -LOD.mDither); }
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
            mDrawRecords[Destination] = FMeshDrawRecord{VisibleObject.mObjectIndex, Templates[TemplateIndex].mMaterialIndex, VisibleObject.mFlags, VisibleObject.mLODDither};
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

float FRenderQueue::CalculateScreenSize(const FRenderSceneObject& Object, const CameraProbe& Camera, float ProjectionScale, bool Perspective) const {
    const DirectX::BoundingSphere& Bounds{Object.mWorldSphereBounds};
    if (Bounds.Radius <= 1e-4f || !std::isfinite(Bounds.Radius)) {
        return 0.0f;
    }

    float ScreenSize{Bounds.Radius * ProjectionScale};
    if (Perspective) {
        // LOD 선택에 쓰는 카메라 깊이만 계산한다.
        const FMatrix& View{Camera.mView};
        const float ViewDepth{Bounds.Center.x * View.M[0][2] + Bounds.Center.y * View.M[1][2] + Bounds.Center.z * View.M[2][2] + View.M[3][2]};
        ScreenSize /= (std::max)(std::abs(ViewDepth), 1e-4f);
    }

    return ScreenSize;
}
