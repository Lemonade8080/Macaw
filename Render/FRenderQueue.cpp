#include "pch.h"
#include "FRenderQueue.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"

bool FMeshDrawItem::HasSameBatch(const FMeshDrawItem& Other) const {
    return mProbe.mPipelineHandle == Other.mProbe.mPipelineHandle && mProbe.mMaterialHandle == Other.mProbe.mMaterialHandle && mProbe.mMeshHandle == Other.mProbe.mMeshHandle && mTextureSignature == Other.mTextureSignature && mMaterialGroupIndex == Other.mMaterialGroupIndex && mFirstIndex == Other.mFirstIndex && mIndexCount == Other.mIndexCount;
}

void FRenderQueue::Build(const IAssetRegistry* Registry, const FSceneRenderData& Scene, const FRenderView& View) {
    mSceneItems.clear();
    mOutlineItems.clear();
    mGizmoItems.clear();
    if (View.IsPassEnabled(ERenderPass::SceneGeometry)) {
        BuildItems(Registry, Scene.mActorProbes, mSceneItems, View.mSettings.mBRenderSky, View.mCamera.mViewFrustum);
    }
    if (View.mSelectedActorHandle.IsValid()) {
        for (FMeshDrawItem& Item : mSceneItems) {
            if (Item.mProbe.mOwnerHandle == View.mSelectedActorHandle) {
                Item.mProbe.mFlags |= static_cast<Uint32>(ERenderObjectFlags::Selected);
            }
        }
    }
    for (std::size_t Index{}; Index < mSceneItems.size(); ++Index) {
        mSceneItems[Index].mModelIndex = static_cast<Uint32>(Index);
    }
    if (View.IsPassEnabled(ERenderPass::SelectionOutline)) {
        for (const FMeshDrawItem& Item : mSceneItems) {
            if ((Item.mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0) {
                mOutlineItems.push_back(Item);
            }
        }
    }
    if (View.IsPassEnabled(ERenderPass::Gizmo)) {
        BuildItems(Registry, View.mGizmoProbes, mGizmoItems, true, View.mCamera.mViewFrustum);
        for (std::size_t Index{}; Index < mGizmoItems.size(); ++Index) {
            mGizmoItems[Index].mModelIndex = static_cast<Uint32>(mSceneItems.size() + Index);
        }
    }
}

const TArray<FMeshDrawItem>& FRenderQueue::GetItems(ERenderPass Pass) const {
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

void FRenderQueue::BuildItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, TArray<FMeshDrawItem>& Items, bool RenderSky, const FFrustum& Frustum) {
    if (Registry == nullptr) {
        return;
    }

    const FAssetHandle SkyPipelineHandle{Registry->FindAsset(FAssetPath{"/Game/Pipeline/SkyDome.json"})};
    for (std::size_t Begin{}; Begin < Probes.size();) {
        const FActorProbe& Source{Probes[Begin]};
        std::size_t End{Begin + 1};
        while (End < Probes.size() && Source.mPipelineHandle == Probes[End].mPipelineHandle && Source.mMaterialHandle == Probes[End].mMaterialHandle && Source.mMeshHandle == Probes[End].mMeshHandle) {
            ++End;
        }

        const UMesh* Mesh{Registry->ResolveAsset<UMesh>(Source.mMeshHandle)};
        if ((RenderSky || Source.mPipelineHandle != SkyPipelineHandle) && Mesh != nullptr && Registry->ResolveAsset<UPipeline>(Source.mPipelineHandle) != nullptr) {
            const TArray<UMesh::FSubMesh>& SubMeshes{Mesh->GetSubMeshes()};
            if (SubMeshes.empty()) {
                AddItems(Registry, Probes, Begin, End, 0, 0, static_cast<Uint32>(Mesh->GetIndices().size()), Items, Frustum);
            } else {
                for (const UMesh::FSubMesh& SubMesh : SubMeshes) {
                    AddItems(Registry, Probes, Begin, End, SubMesh.mMaterialGroupIndex, SubMesh.mFirstIndex, SubMesh.mIndexCount, Items, Frustum);
                }
            }
        }
        Begin = End;
    }
}

void FRenderQueue::AddItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, std::size_t Begin, std::size_t End, Uint32 MaterialGroupIndex, Uint32 FirstIndex, Uint32 IndexCount, TArray<FMeshDrawItem>& Items, const FFrustum& Frustum) {
    const UMaterial* Material{Registry->ResolveAsset<UMaterial>(Probes[Begin].mMaterialHandle)};
    if (Material == nullptr || IndexCount == 0) {
        return;
    }
    const Uint32 GroupIndex{Material->GetGPUIndex(MaterialGroupIndex) != UINT32_MAX ? MaterialGroupIndex : 0u};
    const Uint32 MaterialIndex{Material->GetGPUIndex(GroupIndex)};
    if (MaterialIndex == UINT32_MAX) {
        return;
    }

    const FMaterialChunkSignature TextureSignature{Material->BuildChunkSignature(GroupIndex)};
    for (std::size_t Index{Begin}; Index < End; ++Index) {
        const FActorProbe& Probe{Probes[Index]};
        DirectX::BoundingOrientedBox WorldBounds{};
        Probe.mLocalBounds.Transform(WorldBounds, Probe.mWorld.ToSimpleMath());

        if (!Frustum.Intersects(WorldBounds)) {
            continue;
        }

        Items.push_back(FMeshDrawItem{Probe, TextureSignature, MaterialIndex, GroupIndex, FirstIndex, IndexCount});
    }
}
