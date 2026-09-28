#pragma once

#include <array>

#include "FEditorWindow.h"
#include "Editor/View/FViewportPresetLayout.h"
#include "Core/Channel/FEditorSettings.h"
#include "Editor/View/EditorViewport.h"
#include "Editor/View/FEditorViewport.h"
#include "Editor/Input/FKeyboardInput.h"
#include "Editor/Input/FMouseInput.h"
#include "World/FWorldEditorContext.h"
#include <d3d11.h>

class FStatPanel;

class FViewportHostWindow final : public FEditorWindow {
public:
    using FViewportId = ::FViewportId;
    static constexpr Uint32 MaximumViewportCount{FViewportPresetLayout::MaximumViewportCount};

    FViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext);
    ~FViewportHostWindow() override;

public:
    void SetStatOverlay(std::unique_ptr<FStatPanel> Overlay);
    void PrepareFrame(ImGuiID InDockSpaceId);
    void ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime);
    void ReleaseRenderResources() override;
    FEditorViewport* PrepareViewportForRender(FViewportId Id);
    Uint32 GetViewportCount() const;

    void ApplyLayoutSettings(const FEditorSettings& Settings);
    void CaptureLayoutSettings(FEditorSettings& Settings) const;

private:
    void DrawContents() override;
    void PushWindowStyle() override;
    void SetViewportLayout(EViewportLayoutPreset InPreset);
    bool DrawSplitterHandle(SSplitter& Splitter);
    FEditorViewport* GetViewport(FViewportId Id) const;

private:
    std::unique_ptr<FStatPanel> mStatOverlay{};
    FViewportPresetLayout mLayout{};
    std::array<std::unique_ptr<FEditorViewport>, MaximumViewportCount> mViewports{};
    FViewportId mActiveViewportId{0};
    ImGuiID mDockSpaceId{0};
    bool mBSplitterActive{false};

    FEditorSettings mPendingLayoutSettings{};
    bool mBHasPendingLayoutSettings{true};
};
