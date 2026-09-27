#pragma once

#include "FRenderView.h"
#include "Asset/FMaterialChunkSignature.h"

class IAssetRegistry;

struct FMeshDrawItem {
    FActorProbe mProbe{};
    FMaterialChunkSignature mTextureSignature{};
    Uint32 mMaterialIndex{};
    Uint32 mMaterialGroupIndex{};
    Uint32 mFirstIndex{};
    Uint32 mIndexCount{};
    bool HasSameBatch(const FMeshDrawItem& Other) const;
    bool operator<(const FMeshDrawItem& Other) const;
};

class FRenderQueue {
public:
    void Build(const IAssetRegistry* Registry, const FRenderView& View, const FRenderProbe& Probe);
    const TArray<FMeshDrawItem>& GetItems(ERenderPass Pass) const;

private:
    void BuildItems(const IAssetRegistry* Registry, const TArray<FActorProbe>& Probes, TArray<FMeshDrawItem>& Items, bool RenderSky, bool ForceUnlit);
    void AddItem(const IAssetRegistry* Registry, const FActorProbe& Probe, Uint32 MaterialGroupIndex, Uint32 FirstIndex, Uint32 IndexCount, TArray<FMeshDrawItem>& Items);

private:
    TArray<FMeshDrawItem> mSceneItems{};
    TArray<FMeshDrawItem> mOutlineItems{};
    TArray<FMeshDrawItem> mGizmoItems{};
    TArray<FMeshDrawItem> mEmptyItems{};
};
