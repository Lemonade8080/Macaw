#pragma once

#include "FRenderContext.h"
#include "FRenderQueue.h"

struct FMeshDrawStats {
    Uint64 mPipelineBindCount{};
    Uint64 mTextureBindCount{};
    Uint64 mMeshBindCount{};
    Uint64 mDrawCallCount{};
};

class FMeshRenderer {
public:
    void Draw(const FRenderContext& Context, const TArray<FMeshDrawBatch>& Items, ERenderMode Mode);

    const FMeshDrawStats& GetLastDrawStats() const;

private:
    FMeshDrawStats mLastDrawStats{};
};
