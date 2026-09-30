#pragma once

#include "FRenderContext.h"
#include "FRenderQueue.h"

class FGpuOcclusionCulling;

struct FMeshDrawStats {
    Uint64 mPipelineBindCount{};
    Uint64 mTextureBindCount{};
    Uint64 mMeshBindCount{};
    Uint64 mDrawCallCount{};
};

class FMeshRenderer {
public:
    void Draw(const FRenderContext& Context, const TArray<FMeshDrawBatch>& Items, ERenderMode Mode, const FGpuOcclusionCulling* Occlusion = nullptr);
    void DrawOccluded(const FRenderContext& Context, const FRenderView& View, const FRenderQueue& Queue, FGpuOcclusionCulling& Occlusion);

    const FMeshDrawStats& GetLastDrawStats() const;

private:
    void Execute(const FRenderContext& Context, const TArray<FMeshDrawBatch>& Items, ERenderMode Mode, const FGpuOcclusionCulling* Occlusion, bool RecordStatistics);

private:
    FMeshDrawStats mLastDrawStats{};
};
