#pragma once

#include "FRenderContext.h"
#include "FRenderQueue.h"

class FMeshRenderer {
public:
    void Draw(const FRenderContext& Context, const TArray<FMeshDrawItem>& Items, ERenderMode Mode);
};
