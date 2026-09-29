#pragma once

#include "FRenderView.h"
#include "Asset/FMaterialChunkSignature.h"
#include "FBVHTree.h"

class IAssetRegistry;

struct FMeshDrawItem {
    FActorProbe mProbe{};
    FMaterialChunkSignature mTextureSignature{};
    Uint32 mMaterialIndex{};
    Uint32 mMaterialGroupIndex{};
    Uint32 mFirstIndex{};
    Uint32 mIndexCount{};
    Uint32 mModelIndex{};
    bool HasSameBatch(const FMeshDrawItem& Other) const;
};

class FRenderQueue {
public:
    void Build(const IAssetRegistry* Registry, const FSceneRenderData& Scene, const FRenderView& View);
    const TArray<FMeshDrawItem>& GetItems(ERenderPass Pass) const;

private:
    void BuildItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, TArray<FMeshDrawItem>& Items, bool RenderSky);
    void AddItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, std::size_t Begin, std::size_t End, Uint32 MaterialGroupIndex, Uint32 FirstIndex, Uint32 IndexCount, TArray<FMeshDrawItem>& Items);

    void FrustumCulling(const TArray<FActorProbe>& BeforeCullingProbes, const FFrustum& Frustum);
private:
    TArray<FMeshDrawItem> mSceneItems{};
    TArray<FMeshDrawItem> mOutlineItems{};
    TArray<FMeshDrawItem> mGizmoItems{};
    TArray<FMeshDrawItem> mEmptyItems{};
    
    // ÄÃ¸µµÈ ActorProbes 
    TArray<FActorProbe> mVisibleProbes{};
    FBVHTree mBVHTree;  
    size_t mCachedProbeCount{ 0 };
};
