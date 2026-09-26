#include "pch.h"
#include "FRenderQueue.h"
#include "Asset/FAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"

bool FMeshDrawItem::HasSameBatch(const FMeshDrawItem& Other) const {
    return !(*this < Other) && !(Other < *this);
}

bool FMeshDrawItem::operator<(const FMeshDrawItem& Other) const {
    return std::tie(mProbe.mPipelineHandle.mId, mProbe.mPipelineHandle.mGeneration, mTextureSignature.mTextureFieldCount, mTextureSignature.mTextureHandles, mProbe.mMeshHandle.mId, mProbe.mMeshHandle.mGeneration, mProbe.mMaterialHandle.mId, mProbe.mMaterialHandle.mGeneration, mMaterialGroupIndex, mFirstIndex, mIndexCount) < std::tie(Other.mProbe.mPipelineHandle.mId, Other.mProbe.mPipelineHandle.mGeneration, Other.mTextureSignature.mTextureFieldCount, Other.mTextureSignature.mTextureHandles, Other.mProbe.mMeshHandle.mId, Other.mProbe.mMeshHandle.mGeneration, Other.mProbe.mMaterialHandle.mId, Other.mProbe.mMaterialHandle.mGeneration, Other.mMaterialGroupIndex, Other.mFirstIndex, Other.mIndexCount);
}

void FRenderQueue::Build(FAssetRegistry& Registry, const FRenderView& View, const FRenderProbe& Probe) {
    mSceneItems.clear();
    mOutlineItems.clear();
    mGizmoItems.clear();
    if (View.IsPassEnabled(ERenderPass::SceneGeometry)) {
        BuildItems(Registry, Probe.mActorProbes, mSceneItems, View.mSettings.mBRenderSky, Probe.mBForceUnlit || View.mRenderMode == ERenderMode::Unlit || View.mRenderMode == ERenderMode::Wireframe);
    }
    if (View.IsPassEnabled(ERenderPass::SelectionOutline)) {
        for (const FMeshDrawItem& Item : mSceneItems) {
            if ((Item.mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0) {
                mOutlineItems.push_back(Item);
            }
        }
    }
    if (View.IsPassEnabled(ERenderPass::Gizmo)) {
        BuildItems(Registry, Probe.mGizmoProbes, mGizmoItems, true, true);
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

void FRenderQueue::BuildItems(FAssetRegistry& Registry, const TArray<FActorProbe>& Probes, TArray<FMeshDrawItem>& Items, bool RenderSky, bool ForceUnlit) {
    const FAssetHandle SkyPipelineHandle{Registry.FindAsset(FAssetPath{"/Game/Pipeline/SkyDome.json"})};
    for (const FActorProbe& Source : Probes) {
        if (!RenderSky && Source.mPipelineHandle == SkyPipelineHandle) {
            continue;
        }
        const UMesh* Mesh{Registry.ResolveAsset<UMesh>(Source.mMeshHandle)};
        if (Mesh == nullptr || Registry.ResolveAsset<UPipeline>(Source.mPipelineHandle) == nullptr) {
            continue;
        }
        FActorProbe Probe{Source};
        if (ForceUnlit) {
            Probe.mFlags |= static_cast<Uint32>(ERenderObjectFlags::Unlit);
        }
        const TArray<UMesh::FSubMesh>& SubMeshes{Mesh->GetSubMeshes()};
        if (SubMeshes.empty()) {
            AddItem(Registry, Probe, 0, 0, static_cast<Uint32>(Mesh->GetIndices().size()), Items);
        } else {
            for (const UMesh::FSubMesh& SubMesh : SubMeshes) {
                AddItem(Registry, Probe, SubMesh.mMaterialGroupIndex, SubMesh.mFirstIndex, SubMesh.mIndexCount, Items);
            }
        }
    }
    std::sort(Items.begin(), Items.end());
}

void FRenderQueue::AddItem(FAssetRegistry& Registry, const FActorProbe& Probe, Uint32 MaterialGroupIndex, Uint32 FirstIndex, Uint32 IndexCount, TArray<FMeshDrawItem>& Items) {
    const UMaterial* Material{Registry.ResolveAsset<UMaterial>(Probe.mMaterialHandle)};
    if (Material == nullptr || IndexCount == 0) {
        return;
    }
    const Uint32 GroupIndex{Material->GetGPUIndex(MaterialGroupIndex) != UINT32_MAX ? MaterialGroupIndex : 0u};
    const Uint32 MaterialIndex{Material->GetGPUIndex(GroupIndex)};
    if (MaterialIndex != UINT32_MAX) {
        Items.push_back(FMeshDrawItem{Probe, Material->BuildChunkSignature(GroupIndex), MaterialIndex, GroupIndex, FirstIndex, IndexCount});
    }
}
