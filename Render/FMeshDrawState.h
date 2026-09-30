#pragma once

#include "Asset/FMaterialChunkSignature.h"

class UMaterial;
class UMesh;

struct FMeshDrawState {
    FAssetHandle mPipelineHandle{};
    FAssetHandle mMeshHandle{};
    FMaterialChunkSignature mTextureSignature{};

    Uint32 mFirstIndex{};
    Uint32 mIndexCount{};
    Uint32 mLODLevel{};
};

struct FRenderBatchTemplate {
    FMeshDrawState mState{};

    Uint32 mMaterialIndex{};
};

void AppendMeshDrawTemplates(const UMesh& Mesh, const UMaterial& Material, FAssetHandle PipelineHandle, FAssetHandle MeshHandle, Uint32 LODLevel, TArray<FRenderBatchTemplate>& OutTemplates);
