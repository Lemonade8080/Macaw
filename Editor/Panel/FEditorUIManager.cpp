#include "pch.h"
#include "FEditorUIManager.h"

#include "FPropertyPanel.h"
#include "FConsolePanel.h"
#include "FStatPanel.h"
#include "FViewerPanel.h"
#include "FAssetBrowserPanel.h"
#include "FMaterialEditorPanel.h"
#include "Outliner.h"
#include "FViewportHostWindow.h"
#include "World/FWorldEditorContext.h"
#include "Render/Renderer.h"

FEditorUIManager::FEditorUIManager() = default;
FEditorUIManager::~FEditorUIManager() = default;

void FEditorUIManager::Initialize(UWorld& World, FRenderer& Renderer, FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle, FStateChannel<Uint8>::FReadWriter GizmoSender, FStateChannel<Uint8>::FReadWriter GizmoCoordinateSpaceSender, FAssetThumbnailRenderer* ThumbnailRenderer) {
    const FEditorSettings Settings{EditorContext.GetEditorSettings()};
    if (Settings.mViewportPanelEnabled) {
        AddViewportHostWindow(Renderer.GetDevice(), EditorContext);
    }
    if (Settings.mPropertyPanelEnabled) {
        AddWindow(std::make_unique<FPropertyPanel>(EditorContext, std::move(GizmoSender), std::move(GizmoCoordinateSpaceSender), ThumbnailRenderer));
    }
    if (Settings.mConsolePanelEnabled) {
        AddWindow(std::make_unique<FConsolePanel>(Console::STDOutHandle, mStatDisplayChannel.GetWriter()));
    }
    if (Settings.mStatPanelEnabled && mViewportHostWindow != nullptr) {
        mViewportHostWindow->SetStatOverlay(std::make_unique<FStatPanel>(mStatDisplayChannel.GetReader()));
    }
    std::unique_ptr<FMaterialEditorPanel> MaterialWindow{};
    if (Settings.mMaterialEditorPanelEnabled && ThumbnailRenderer != nullptr) {
        MaterialWindow = std::make_unique<FMaterialEditorPanel>(AssetRegistry, *ThumbnailRenderer);
        mMaterialEditorPanel = MaterialWindow.get();
    }
    if (Settings.mAssetBrowserPanelEnabled) {
        std::unique_ptr<FAssetBrowserPanel> AssetBrowserWindow{std::make_unique<FAssetBrowserPanel>(AssetRegistry, EditorContext, ThumbnailRenderer, [this](FAssetHandle Handle) {
            if (mMaterialEditorPanel != nullptr) {
                mMaterialEditorPanel->OpenMaterial(Handle);
                mFocusMaterialEditor = true;
            }
        })};
        mAssetBrowserPanel = AssetBrowserWindow.get();
        AddWindow(std::move(AssetBrowserWindow));
    }
    if (MaterialWindow != nullptr) {
        AddWindow(std::move(MaterialWindow));
    }
    if (Settings.mOutlinerPanelEnabled) {
        AddWindow(std::make_unique<FOutlinerPanel>(World, EditorContext));
    }
    AddViewerWindow(AssetRegistry, EditorContext, WindowHandle, ThumbnailRenderer);
}

void FEditorUIManager::InitializeViewer(FAssetRegistry& AssetRegistry, HWND WindowHandle, FWorldEditorContext& EditorContext, FAssetThumbnailRenderer* ThumbnailRenderer) {
    AddViewerWindow(AssetRegistry, EditorContext, WindowHandle, ThumbnailRenderer);
}

void FEditorUIManager::Tick() {
    mDockSpaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    if (mViewerWindow != nullptr && mPreviewContext != nullptr && mPreviewContext->ConsumePreviewOpenRequest()) {
        mViewerWindow->SetVisible(true);
        ImGui::SetWindowFocus(mViewerWindow->GetWindowName());
    }
    if (mViewportHostWindow != nullptr) {
        mViewportHostWindow->PrepareFrame(mDockSpaceId);
    }
    if (mMaterialEditorPanel != nullptr && !mMaterialEditorPanel->IsVisible()) {
        mMaterialEditorPanel->ReleaseRenderResources();
    }
    for (const std::unique_ptr<IEditorPanel>& Element : mElements) {
        if (Element != nullptr && Element->IsVisible()) {
            Element->DrawPanel();
        }
    }
    if (mMaterialEditorPanel != nullptr && mMaterialEditorPanel->IsVisible() && mFocusMaterialEditor) {
        ImGui::SetWindowFocus(mMaterialEditorPanel->GetWindowName());
        mFocusMaterialEditor = false;
    }
}

void FEditorUIManager::RenderOffscreen(FRenderer& Renderer, FAssetRegistry& AssetRegistry) {
    for (const std::unique_ptr<IEditorPanel>& Element : mElements) {
        if (Element != nullptr && Element->IsVisible()) {
            Element->RenderOffscreen(Renderer, AssetRegistry);
        }
    }
}

void FEditorUIManager::ReleaseRenderResources() {
    for (const std::unique_ptr<IEditorPanel>& Element : mElements) {
        if (Element != nullptr) {
            Element->ReleaseRenderResources();
        }
    }
}

ImGuiID FEditorUIManager::GetDockSpaceId() const {
    return mDockSpaceId;
}

const std::vector<FEditorWindow*>& FEditorUIManager::GetWindows() const {
    return mWindows;
}

FViewportHostWindow* FEditorUIManager::GetViewportHostWindow() const {
    return mViewportHostWindow;
}

bool FEditorUIManager::HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition) {
#ifdef OBJ_VIEWER
    return mViewerWindow != nullptr && mViewerWindow->HandleExternalFileDrop(FilePath, ScreenPosition);
#else
    return mAssetBrowserPanel != nullptr && mAssetBrowserPanel->IsVisible() && mAssetBrowserPanel->HandleExternalFileDrop(FilePath, ScreenPosition);
#endif
}

void FEditorUIManager::AddPanel(std::unique_ptr<IEditorPanel> Panel) {
    mElements.emplace_back(std::move(Panel));
}

void FEditorUIManager::AddWindow(std::unique_ptr<FEditorWindow> Window) {
    mWindows.emplace_back(Window.get());
    mElements.emplace_back(std::move(Window));
}

void FEditorUIManager::AddViewerWindow(FAssetRegistry& AssetRegistry, FWorldEditorContext& EditorContext, HWND WindowHandle, FAssetThumbnailRenderer* ThumbnailRenderer) {
    if (!EditorContext.GetEditorSettings().mViewerPanelEnabled) {
        return;
    }

    std::unique_ptr<FViewerPanel> Window{std::make_unique<FViewerPanel>(AssetRegistry, WindowHandle, EditorContext.GetEditorToWorldSender(), EditorContext, ThumbnailRenderer)};
    mViewerWindow = Window.get();
    mPreviewContext = &EditorContext;
    AddWindow(std::move(Window));
}

void FEditorUIManager::AddViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext) {
    std::unique_ptr<FViewportHostWindow> Window{std::make_unique<FViewportHostWindow>(Device, EditorContext)};
    mViewportHostWindow = Window.get();
    AddWindow(std::move(Window));
}
