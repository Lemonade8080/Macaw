#pragma once

#include "PCH.h"
#include "ImGui/imgui.h"
#include "IEditorPanel.h"
#include "Core/Channel/FEditorInfo.h"
#include "Core/Channel/FMessageChannel.h"
#include "World/FWorldEditorContext.h"

class FControlPanel : public IEditorPanel {
public:
    FControlPanel(FWorldEditorContext& InEditorContext, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender);

public:
    void DrawPanel() override;

private:
    FWorldEditorContext* mEditorContext{nullptr};
    FMessageChannel::FSender mEditorToWorldSender;

    char mSceneNameBuffer[256]{"NewScene"};

    int mSelectedComponentIndex{-1};
    int mSelectedMeshIndex{0};
    int mSpawnCountToRequest{1};

    std::size_t mRenderModeIndex{0};

    // Components 체크리스트에서 컴포넌트 타입을 검색한다.
    ImGuiTextFilter mComponentFilter{};

    HWND mWindowHandle{};
};
