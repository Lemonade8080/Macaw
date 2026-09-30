#pragma once

#include <d3d11.h>
#include <cstddef>
#include <memory>

#include "Asset/FAssetRegistry.h"
#include "../../Core/Base/FRenderProbe.h"

#include "Render/FSceneRenderSurface.h"
#include "Render/Renderer.h"
#include "Asset/UMesh.h"

class FAssetThumbnailRenderer {
private:
    struct FThumbnail {
        std::unique_ptr<FSceneRenderSurface> mSurface{};
    };

public:
    FAssetThumbnailRenderer() = default;
    ~FAssetThumbnailRenderer() = default;

    FAssetThumbnailRenderer(const FAssetThumbnailRenderer&) = delete;
    FAssetThumbnailRenderer& operator=(const FAssetThumbnailRenderer&) = delete;

    FAssetThumbnailRenderer(FAssetThumbnailRenderer&&) = delete;
    FAssetThumbnailRenderer& operator=(FAssetThumbnailRenderer&&) = delete;

public:
    void Create(FRenderer* InRenderer, FAssetRegistry* InAssetRegistry);
    void Tick(Uint32 MaxThumbnailCount = 1);
    float GetGenerationProgress() const;

    void RenderThumbnail(FAssetHandle AssetHandle);
    void RenderMaterialPreview(FAssetHandle MaterialHandle, FSceneRenderSurface& Surface);

    ID3D11ShaderResourceView* GetThumbnail(FAssetHandle AssetHandle) const;

    void Terminate();

private:
    void RenderThumbnail(const FAssetEntry& Entry, FSceneRenderSurface* PreviewSurface = nullptr);

    FMatrix BuildMeshTransform(const UMesh& Mesh) const;
    CameraProbe BuildCamera() const;
    FMatrix MakeCameraWorldMatrix(const FVector& Eye, const FVector& Target) const;

    const FAssetEntry* FindAssetEntry(FAssetHandle AssetHandle) const;

    static Uint64 MakeThumbnailKey(FAssetHandle AssetHandle);

private:
    FRenderer* mRenderer{nullptr};
    FAssetRegistry* mAssetRegistry{nullptr};
    Uint64 mRenderSceneId{AllocateRenderSceneId()};
    Uint64 mRenderSceneRevision{};

    TMap<Uint64, FThumbnail> mThumbnails{};
    TArray<FAssetHandle> mPendingAssetHandles{};
    std::size_t mPendingAssetIndex{};
};
