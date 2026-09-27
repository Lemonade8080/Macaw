#pragma once

#include "Core/Channel/FEditorInfo.h"
#include "Core/Channel/FStateChannel.h"

struct ImVec2;

class FStatPanel {
public:
    explicit FStatPanel(FStateChannel<FStatDisplayFlags>::FReader InReader);

public:
    void Draw(const ImVec2& Min, const ImVec2& Max);

private:
    FStateChannel<FStatDisplayFlags>::FReader mModeReader{};
};
