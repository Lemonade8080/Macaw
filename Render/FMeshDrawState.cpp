#include "pch.h"
#include "FMeshDrawState.h"

#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"

void AppendMeshDrawTemplates(const UMesh& Mesh, const UMaterial& Material, FAssetHandle PipelineHandle, FAssetHandle MeshHandle, Uint32 LODLevel, TArray<FRenderBatchTemplate>& OutTemplates) {
    const TArray<UMesh::FSubMesh>& SubMeshes{Mesh.GetSubMeshes(static_cast<int>(LODLevel))};
    const bool UseSubMeshes{!SubMeshes.empty()};
    const std::size_t SectionCount{UseSubMeshes ? SubMeshes.size() : 1};

    for (std::size_t SectionIndex{}; SectionIndex < SectionCount; ++SectionIndex) {
        const Uint32 RequestedGroup{UseSubMeshes ? SubMeshes[SectionIndex].mMaterialGroupIndex : 0};
        const Uint32 MaterialGroup{Material.GetGPUIndex(RequestedGroup) != UINT32_MAX ? RequestedGroup : 0};
        const Uint32 MaterialIndex{Material.GetGPUIndex(MaterialGroup)};
        const Uint32 FirstIndex{UseSubMeshes ? SubMeshes[SectionIndex].mFirstIndex : 0};
        const Uint32 IndexCount{UseSubMeshes ? SubMeshes[SectionIndex].mIndexCount : Mesh.GetIndexCount(static_cast<int>(LODLevel))};
        if (MaterialIndex == UINT32_MAX || IndexCount == 0) {
            continue;
        }

        FMeshDrawState State{};
        State.mPipelineHandle = PipelineHandle;
        State.mMeshHandle = MeshHandle;
        State.mTextureSignature = Material.BuildChunkSignature(MaterialGroup);
        State.mFirstIndex = FirstIndex;
        State.mIndexCount = IndexCount;
        State.mLODLevel = LODLevel;
        State.mOriginalIndexCount = UseSubMeshes ?
            (SubMeshes[SectionIndex].mSourceIndexCount != 0 ? SubMeshes[SectionIndex].mSourceIndexCount : IndexCount) : Mesh.GetIndexCount(0);

        OutTemplates.push_back(FRenderBatchTemplate{State, MaterialIndex});
    }
}
