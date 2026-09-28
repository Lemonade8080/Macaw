#include "pch.h"
#include "FViewerToolBar.h"
#include "ImGui/imgui.h"

void FViewerToolBar::DrawPanel() {
    if (ImGui::BeginMenu("Import")) {
        if (ImGui::MenuItem("Open Obj...")) {
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    const ERenderMode RenderModeValues[]{ERenderMode::Lit, ERenderMode::Unlit, ERenderMode::Wireframe};
    int RenderIndex{};
    for (int Index{}; Index < IM_ARRAYSIZE(RenderModeValues); ++Index) {
        if (mEditorContext->GetRenderModeState() == static_cast<std::size_t>(RenderModeValues[Index])) {
            RenderIndex = Index;
            break;
        }
    }
    const char* RenderModes[]{"Lit", "Unlit", "Wireframe"};
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Render Mode", &RenderIndex, RenderModes, IM_ARRAYSIZE(RenderModes))) {
        mEditorContext->SetRenderModeState(static_cast<std::size_t>(RenderModeValues[RenderIndex]));
    }
}

FViewerToolBar::FViewerToolBar(FWorldEditorContext& InEditorContext)
	: mEditorContext{&InEditorContext} {
}
