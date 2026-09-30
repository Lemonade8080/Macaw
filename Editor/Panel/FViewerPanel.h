#pragma once

#include "PCH.h"
#include "FEditorWindow.h"
#include "FPropertyEditorContext.h"
#include "Render/FSceneRenderSurface.h"
#include "Core/Base/FRenderProbe.h"
#include "Core/Channel/FMessageChannel.h"
#include "Core/Base/FAssetHandle.h"
#include "World/FWorldEditorContext.h"
#include "Render/Renderer.h"
#include "Asset/FAssetRegistry.h"
#include "Editor/View/FAssetThumbnailRenderer.h"

class FViewerPanel final : public FEditorWindow {
public:
    FViewerPanel(FAssetRegistry& InRegistry, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender, FWorldEditorContext& InEditorContext, FAssetThumbnailRenderer* InThumbnailRenderer);
    ~FViewerPanel() override = default;

    FViewerPanel(const FViewerPanel&) = delete;
    FViewerPanel& operator=(const FViewerPanel&) = delete;
    FViewerPanel(FViewerPanel&&) = delete;
    FViewerPanel& operator=(FViewerPanel&&) = delete;

public:
    void RenderOffscreen(FRenderer& InRenderer, FAssetRegistry& InRegistry) override;
    void ReleaseRenderResources() override;

    void SetMesh(FAssetHandle InMeshHandle);
    void SetMaterial(FAssetHandle InMaterialHandle);
    bool HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition);

private:
    void DrawContents() override;
    void DrawMenuBar();
    void DrawProperties();
    void DrawPreview();

    void ResizeSurfaceIfNeeded(ID3D11Device* Device, Uint32 Width, Uint32 Height);
    FSceneRenderData BuildPreviewScene();
    CameraProbe BuildPreviewCamera() const;

    void ProcessInput();
    FMatrix MakeCameraWorldMatrix(const FVector3& Eye) const;
    bool OpenViewerFile(const std::filesystem::path& FilePath);

private:
    FAssetRegistry* mRegistry{nullptr};
    FMessageChannel::FSender mEditorToWorldSender;
    HWND mWindowHandle{nullptr};
    FWorldEditorContext& mEditorContext;
    FPropertyEditorContext mPropertyEditor{};

    FSceneRenderSurface mSurface{};
    Uint64 mRenderSceneId{AllocateRenderSceneId()};
    Uint64 mRenderSceneRevision{};
    FAssetHandle mMeshHandle{};
    FAssetHandle mMaterialHandle{};

    Uint32 mSurfaceWidth{};
    Uint32 mSurfaceHeight{};
    Uint32 mDesiredWidth{};
    Uint32 mDesiredHeight{};

    float mDistance{5.0f};
    FVector3 mTarget{0.0f, 0.0f, 0.0f};
    float mFieldOfView{1.0472f};
    FQuat mOrbitRotation{};

    ImVec2 mDropTargetMin{};
    ImVec2 mDropTargetMax{};
};
