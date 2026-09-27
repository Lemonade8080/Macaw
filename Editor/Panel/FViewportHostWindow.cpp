#include "pch.h"

#include "FViewportHostWindow.h"
#include "FStatPanel.h"

#include "Editor/Input/FKeyboardInput.h"
#include "Editor/Input/FMouseInput.h"
#include "Editor/View/FEditorViewport.h"
#include "Editor/View/EditorViewport.h"

FViewportHostWindow::FViewportHostWindow(ID3D11Device* Device, FWorldEditorContext& EditorContext)
    : FEditorWindow("Viewports###SplitSceneViewport", ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse),
      mLayout(EViewportLayoutPreset::FourGrid),
      mPendingLayoutSettings(EditorContext.GetEditorSettings()),
      mBHasPendingLayoutSettings(true) {
    for (FViewportId Id{0}; Id < MaximumViewportCount; ++Id) {
        mViewports[Id] = std::make_unique<FEditorViewport>(Id, Device, EditorContext);
    }
}

FViewportHostWindow::~FViewportHostWindow() = default;

void FViewportHostWindow::SetStatOverlay(std::unique_ptr<FStatPanel> Overlay) {
    mStatOverlay = std::move(Overlay);
}

void FViewportHostWindow::PrepareFrame(ImGuiID InDockSpaceId) {
    mDockSpaceId = InDockSpaceId;
    mBSplitterActive = false;

    for (const std::unique_ptr<FEditorViewport>& Viewport : mViewports) {
        Viewport->BeginFrame();
    }
}

void FViewportHostWindow::ProcessInput(EditorViewport& Viewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime) {
    GetViewport(mActiveViewportId)->ProcessInput(Viewport, KeyboardInput, MouseInput, DeltaTime, mBSplitterActive);
}

void FViewportHostWindow::ReleaseRenderResources() {
    for (const std::unique_ptr<FEditorViewport>& Viewport : mViewports) {
        Viewport->ReleaseRenderResources();
    }
}

FEditorViewport* FViewportHostWindow::PrepareViewportForRender(FViewportId Id) {
    FEditorViewport* Viewport{GetViewport(Id)};
    return Viewport != nullptr && Viewport->PrepareForRender() ? Viewport : nullptr;
}

Uint32 FViewportHostWindow::GetViewportCount() const {
    return mLayout.GetViewportCount();
}

void FViewportHostWindow::ApplyLayoutSettings(const FEditorSettings& Settings) {
    // 기본은 4분할
    EViewportLayoutPreset Preset{EViewportLayoutPreset::FourGrid};

    if (Settings.mViewportLayoutPreset < static_cast<Uint8>(EViewportLayoutPreset::Count)) {
        Preset = static_cast<EViewportLayoutPreset>(Settings.mViewportLayoutPreset);
    }

    mLayout.SetPreset(Preset);

    const std::array<float, FViewportPresetLayout::MaximumSplitterCount> Ratios{ Settings.mViewportSplitterRatio0, Settings.mViewportSplitterRatio1, Settings.mViewportSplitterRatio2};

    mLayout.RestoreSplitterRatios(Ratios, Settings.mViewportSplitterCount);

    mActiveViewportId = 0;
}

void FViewportHostWindow::CaptureLayoutSettings(FEditorSettings& Settings) const {
    std::array<float, FViewportPresetLayout::MaximumSplitterCount> Ratios{0.5f, 0.5f, 0.5f};

    Uint32 RatioCount{0};

    mLayout.GetSplitterRatios(Ratios, RatioCount);

    Settings.mViewportLayoutPreset = static_cast<Uint8>(mLayout.GetPreset());

    Settings.mViewportSplitterCount = RatioCount;
    Settings.mViewportSplitterRatio0 = Ratios[0];
    Settings.mViewportSplitterRatio1 = Ratios[1];
    Settings.mViewportSplitterRatio2 = Ratios[2];
}

void FViewportHostWindow::DrawContents() {
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("Layout", mLayout.GetPresetName())) {
        for (Uint8 Index{0}; Index < static_cast<Uint8>(EViewportLayoutPreset::Count); ++Index) {
            const EViewportLayoutPreset Preset{static_cast<EViewportLayoutPreset>(Index)};
            const bool BSelected{mLayout.GetPreset() == Preset};
            if (ImGui::Selectable(FViewportPresetLayout::GetPresetName(Preset), BSelected)) {
                SetViewportLayout(Preset);
            }
            if (BSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    const ImVec2 MainViewportPosition{ImGui::GetMainViewport()->Pos};
    const ImVec2 Origin{ImGui::GetCursorScreenPos()};
    const ImVec2 AvailableSize{ImGui::GetContentRegionAvail()};
    const Int32 Width{static_cast<Int32>(std::max(0.0f, AvailableSize.x))};
    const Int32 Height{static_cast<Int32>(std::max(0.0f, AvailableSize.y))};

    if (Width <= 0 || Height <= 0) {
        return;
    }

    const FPoint Min{static_cast<Int32>(Origin.x), static_cast<Int32>(Origin.y)};
    mLayout.SetRect({Min, {Min.mX + Width, Min.mY + Height}});

    // 한번만 실행
    if (mBHasPendingLayoutSettings) {
        ApplyLayoutSettings(mPendingLayoutSettings);
        mBHasPendingLayoutSettings = false;
    }

    std::vector<SSplitter*> Splitters{};
    mLayout.CollectSplitters(Splitters);
    for (SSplitter* Splitter : Splitters) {
        mBSplitterActive = DrawSplitterHandle(*Splitter) || mBSplitterActive;
    }

    if (mBSplitterActive) {
        mLayout.RefreshLayout();
    }

    for (FViewportId Id{0}; Id < GetViewportCount(); ++Id) {
        FEditorViewport* Viewport{GetViewport(Id)};
        if (Viewport != nullptr && Viewport->Draw(mLayout.GetViewportRect(Id), MainViewportPosition, mBSplitterActive, Id == 0 ? mStatOverlay.get() : nullptr)) {
            mActiveViewportId = Viewport->GetViewportId();
        }
    }

    const bool BHostFocused{ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)};
    for (FViewportId Id{0}; Id < GetViewportCount(); ++Id) {
        FEditorViewport* Viewport{GetViewport(Id)};
        if (Viewport != nullptr) {
            Viewport->SetFocused(BHostFocused && Id == mActiveViewportId);
        }
    }
}

void FViewportHostWindow::PushWindowStyle() {
    if (mDockSpaceId != 0) {
        ImGui::SetNextWindowDockID(mDockSpaceId, ImGuiCond_FirstUseEver);
    }
}

void FViewportHostWindow::SetViewportLayout(EViewportLayoutPreset InPreset) {
    if (mLayout.GetPreset() == InPreset) {
        return;
    }

    mLayout.SetPreset(InPreset);

    mActiveViewportId = 0;
    for (const std::unique_ptr<FEditorViewport>& Viewport : mViewports) {
        Viewport->BeginFrame();
    }
    mBSplitterActive = false;
}

bool FViewportHostWindow::DrawSplitterHandle(SSplitter& Splitter) {
    const FRect Rect{Splitter.GetHandleRect()};
    if (Rect.IsEmpty()) {
        return false;
    }

    ImGui::SetCursorScreenPos(ImVec2(static_cast<float>(Rect.mMin.mX), static_cast<float>(Rect.mMin.mY)));
    ImGui::PushID(&Splitter);
    ImGui::InvisibleButton("##ViewportSplitter", ImVec2(static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight())));
    ImGui::PopID();

    const bool BHovered{ImGui::IsItemHovered()};
    const bool BActive{ImGui::IsItemActive()};
    if (BHovered || BActive) {
        const ImGuiMouseCursor Cursor{Rect.GetWidth() < Rect.GetHeight() ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS};
        ImGui::SetMouseCursor(Cursor);
    }

    if (BActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 MousePosition{ImGui::GetMousePos()};
        Splitter.DragTo({static_cast<Int32>(MousePosition.x), static_cast<Int32>(MousePosition.y)});
    }

    const FRect UpdatedRect{Splitter.GetHandleRect()};
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(static_cast<float>(UpdatedRect.mMin.mX), static_cast<float>(UpdatedRect.mMin.mY)), ImVec2(static_cast<float>(UpdatedRect.mMax.mX), static_cast<float>(UpdatedRect.mMax.mY)), BActive ? IM_COL32(100, 150, 220, 255) : BHovered ? IM_COL32(85, 85, 85, 255) : IM_COL32(55, 55, 55, 255));
    return BActive;
}

FEditorViewport* FViewportHostWindow::GetViewport(FViewportId Id) const {
    return Id < MaximumViewportCount ? mViewports[Id].get() : nullptr;
}
