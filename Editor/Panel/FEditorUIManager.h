#pragma once

#include "PCH.h"
#include "ImGui/imgui.h"
#include "Core/Channel/FEditorInfo.h"
#include "FViewportHostWindow.h"
#include "Core/Channel/FStateChannel.h"
#include <filesystem>
#include "World/UWorld.h"
#include "Render/Renderer.h"
#include "Asset/FAssetRegistry.h"
#include "World/FWorldEditorContext.h"
#include "Editor/View/FAssetThumbnailRenderer.h"
#include "Editor/Panel/FAssetBrowserPanel.h"
#include "Editor/Panel/FMaterialEditorPanel.h"
#include "Editor/Panel/FViewerPanel.h"
#include "Editor/Panel/FStatPanel.h"
#include "Editor/Panel/FEditorWindow.h"
#include "Editor/Panel/IEditorPanel.h"

class FEditorUIManager {
public:
    FEditorUIManager();
    ~FEditorUIManager();

public:
    void Initialize(UWorld& World, FRenderer& Renderer, FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle, FStateChannel<Uint8>::FReadWriter GizmoSender, FStateChannel<Uint8>::FReadWriter GizmoCoordinateSpaceSender, FAssetThumbnailRenderer* ThumbnailRenderer);
    void InitializeViewer(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext, FAssetThumbnailRenderer* ThumbnailRenderer);
    void Tick();
    void RenderOffscreen(FRenderer& Renderer, FAssetRegistry& AssetRegistry);
    void ReleaseRenderResources();
    ImGuiID GetDockSpaceId() const;
    const std::vector<FEditorWindow*>& GetWindows() const;
    FViewportHostWindow* GetViewportHostWindow() const;
    bool HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition);

private:
    void AddPanel(std::unique_ptr<IEditorPanel> Panel);
    void AddWindow(std::unique_ptr<FEditorWindow> Window);
    void AddViewerWindow(FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle, FAssetThumbnailRenderer* ThumbnailRenderer);
    void AddViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext);

private:
    std::vector<std::unique_ptr<IEditorPanel>> mElements{};
    std::vector<FEditorWindow*> mWindows{};
    FViewportHostWindow* mViewportHostWindow{};
    FViewerPanel* mViewerWindow{};
    FWorldEditorContext* mPreviewContext{};
    ImGuiID mDockSpaceId{};
    FAssetBrowserPanel* mAssetBrowserPanel{};
    FMaterialEditorPanel* mMaterialEditorPanel{};
    bool mFocusMaterialEditor{};
    FStateChannel<FStatDisplayFlags> mStatDisplayChannel{FStatDisplayFlags{false, false, false}};
};
