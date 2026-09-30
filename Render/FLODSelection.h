#pragma once

#include "Asset/FLODSettings.h"
#include <algorithm>
#include <cmath>

struct FLODSelection {
    Uint32 mLevel{};
    Uint32 mNextLevel{UINT32_MAX};
    // 0: fully visible. Positive: outgoing pixels, negative: complementary incoming pixels.
    float mDither{};
    bool mCulled{};
};

inline FLODSelection SelectMeshLOD(float ScreenSize, float ViewportHeight, Uint32 AvailableMask, bool AllowCull) {
    FLODSelection Result{};
    if (!std::isfinite(ScreenSize) || ScreenSize <= 0.0f) { return Result; }
    if (AllowCull && ViewportHeight > 0.0f) {
        const float Pixels{ScreenSize * ViewportHeight};
        if (ScreenSize <= GLODCullPixels / ViewportHeight) { Result.mCulled = true; return Result; }
        if (Pixels < GLODFadePixels) {
            Result.mDither = (GLODFadePixels - Pixels) / (GLODFadePixels - GLODCullPixels);
        }
    }
    Uint32 Level{};
    for (Uint32 Next{1}; Next < GLODCount; ++Next) {
        if ((AvailableMask & (1u << Next)) == 0) { continue; }
        const float Threshold{GLODSettings[Next - 1].mScreenSize};
        const float Upper{Threshold * (1.0f + GLODTransitionHalfWidth)};
        const float Lower{Threshold * (1.0f - GLODTransitionHalfWidth)};
        if (ScreenSize >= Upper) { break; }
        if (ScreenSize > Lower) {
            // Cull fade takes precedence; never draw more than two copies of an object.
            if (Result.mDither == 0.0f) {
                Result.mNextLevel = Next;
                Result.mDither = std::clamp((Upper - ScreenSize) / (Upper - Lower), 0.0f, 1.0f);
            }
            break;
        }
        Level = Next;
    }
    Result.mLevel = Level;
    return Result;
}
