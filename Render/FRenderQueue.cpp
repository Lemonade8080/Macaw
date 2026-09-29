#include "pch.h"
#include "FRenderQueue.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/FLODSettings.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"

namespace
{
    Uint32 SelectLODLevel(
        const FActorProbe& Probe,
        const UMesh& Mesh,
        const CameraProbe& Camera)
    {
        DirectX::BoundingOrientedBox WorldBounds{};
        Probe.mLocalBounds.Transform(WorldBounds, Probe.mWorld.ToSimpleMath());

        const FVector3 Center{
            WorldBounds.Center.x,
            WorldBounds.Center.y,
            WorldBounds.Center.z
        };

        const FVector3 Extents{
            WorldBounds.Extents.x,
            WorldBounds.Extents.y,
            WorldBounds.Extents.z
        };

        const float Radius{ Extents.Length() };
        if (Radius <= 1e-4f) { return 0; }

        const FVector3 ViewCenter{Camera.mView.TransformPosition(Center)};
        const float ProjectionScale{std::abs(Camera.mProjection.M[1][1])};
        const bool BPerspective{std::abs(Camera.mProjection.M[2][3]) > 1e-6f};
        const float ScreenSize{BPerspective ? Radius * ProjectionScale / (std::max)(std::abs(ViewCenter.Z), 1e-4f) : Radius * ProjectionScale};

        Uint32 Level{GLODCount - 1};

        // 화면에서 차지하는 크기에 맞는 LOD를 선택한다.
        for (Uint32 Index{}; Index < GLODCount; ++Index)
        {
            if (ScreenSize >= GLODSettings[Index].mScreenSize)
            {
                Level = Index;
                break;
            }
        }

        // 요청한 LOD가 없으면 가장 가까운 상위 품질 LOD를 사용한다.
        while (Level > 0 && !Mesh.HasLOD(Level))
        {
            --Level;
        }

        return Level;
    }
}

bool FMeshDrawItem::HasSameBatch(const FMeshDrawItem& Other) const {
    return mProbe.mPipelineHandle == Other.mProbe.mPipelineHandle && mProbe.mMaterialHandle == Other.mProbe.mMaterialHandle && mProbe.mMeshHandle == Other.mProbe.mMeshHandle && mTextureSignature == Other.mTextureSignature && mMaterialGroupIndex == Other.mMaterialGroupIndex && mFirstIndex == Other.mFirstIndex && mIndexCount == Other.mIndexCount && mLODLevel == Other.mLODLevel;
}

void FRenderQueue::Build(const IAssetRegistry* Registry, const FSceneRenderData& Scene, const FRenderView& View) {
    mSceneItems.clear();
    mOutlineItems.clear();
    mGizmoItems.clear();
    
    if (View.IsPassEnabled(ERenderPass::SceneGeometry)) {
        FrustumCulling(Scene.mActorProbes, View.mCamera.mViewFrustum);

        for (const auto& Probe : Scene.mActorProbes) {
            DirectX::BoundingOrientedBox WorldBounds{};
            Probe.mLocalBounds.Transform(WorldBounds, Probe.mWorld.ToSimpleMath());

            if (!View.mCamera.mViewFrustum.Intersects(WorldBounds)) {
                continue;
            }
            PassProbes.push_back(Probe);
        }

        BuildItems(Registry, PassProbes, mSceneItems, View.mSettings.mBRenderSky, View.mCamera, true);
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
        BuildItems(Registry, View.mGizmoProbes, mGizmoItems, true, View.mCamera, false);
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

void FRenderQueue::BuildItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, TArray<FMeshDrawItem>& Items, bool RenderSky, const CameraProbe& Camera, bool BUseLOD) {
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
            TArray<Uint32> LODLevels{};
            LODLevels.reserve(End - Begin);

            for (std::size_t Index{Begin}; Index < End; ++Index) {
                const bool BSelectLOD{BUseLOD && Source.mPipelineHandle != SkyPipelineHandle};
                LODLevels.push_back(BSelectLOD ? SelectLODLevel(Probes[Index], *Mesh, Camera) : 0);
            }

            TArray<FActorProbe> LODProbes{};
            LODProbes.reserve(End - Begin);

            for (Uint32 Level{}; Level < GLODCount; ++Level) {
                LODProbes.clear();

                for (std::size_t Index{Begin}; Index < End; ++Index) {
                    if (LODLevels[Index - Begin] == Level) { LODProbes.push_back(Probes[Index]); }
                }

                if (LODProbes.empty()) { continue; }

                if (Level > 0) {
                    AddItems(Registry, LODProbes, 0, LODProbes.size(), 0, 0, Mesh->GetIndexCount(Level), Level, Items);
                    continue;
                }

                const TArray<UMesh::FSubMesh>& SubMeshes{Mesh->GetSubMeshes()};
                if (SubMeshes.empty()) {
                    AddItems(Registry, LODProbes, 0, LODProbes.size(), 0, 0, static_cast<Uint32>(Mesh->GetIndices().size()), 0, Items);
                } else {
                    for (const UMesh::FSubMesh& SubMesh : SubMeshes) {
                        AddItems(Registry, LODProbes, 0, LODProbes.size(), SubMesh.mMaterialGroupIndex, SubMesh.mFirstIndex, SubMesh.mIndexCount, 0, Items);
                    }
                }
            }
        }
        Begin = End;
    }
}

void FRenderQueue::AddItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, std::size_t Begin, std::size_t End, Uint32 MaterialGroupIndex, Uint32 FirstIndex, Uint32 IndexCount, Uint32 LODLevel, TArray<FMeshDrawItem>& Items) {
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
        Items.push_back(FMeshDrawItem{Probe, TextureSignature, MaterialIndex, GroupIndex, FirstIndex, IndexCount, LODLevel});
    }
}

void FRenderQueue::FrustumCulling(const TArray<FActorProbe>& BeforeCullingProbes, const FFrustum& Frustum)
{
    mVisibleProbes.clear();
    mVisibleProbes.reserve(BeforeCullingProbes.size());

    if (BeforeCullingProbes.empty())
    {
        return;
    }

    const bool bNeedsRebuiled = mBVHTree.GetNodes().empty() || (mCachedProbeCount != BeforeCullingProbes.size());

    if (bNeedsRebuiled)
    {
        mBVHTree.Build(BeforeCullingProbes);
        mCachedProbeCount = BeforeCullingProbes.size();
    }
    
    mBVHTree.FrustumCull(Frustum, BeforeCullingProbes, mVisibleProbes);
}
