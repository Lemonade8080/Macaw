#include "pch.h"
#include "FStatPanel.h"
#include "Editor/Panel/Stats/StatWindow.h"

FStatPanel::FStatPanel(FStateChannel<FStatDisplayFlags>::FReader InReader)
    : mModeReader{std::move(InReader)} {
    //bVisible = false;
}

void FStatPanel::Draw(const ImVec2& Min, const ImVec2& Max) {
    DrawStatOverlay(Min, Max, mModeReader.Peek());
}
