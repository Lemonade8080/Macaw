#pragma once

#include "Core/Common.h"

struct FLODSetting
{
    float mScreenSize;
    float mTargetRatio;
};

// 배열 인덱스가 LOD Level이다.
// ScreenSize는 화면 높이에서 메시가 차지하는 최소 비율이고, TargetRatio는 남길 삼각형 비율이다.
inline constexpr FLODSetting GLODSettings[]{
    { 0.5f, 1.0f },
    { 0.25f, 0.5f },
    { 0.12f, 0.3f },
    { 0.06f, 0.1f },
};

inline constexpr Uint32 GLODCount{static_cast<Uint32>(sizeof(GLODSettings) / sizeof(GLODSettings[0]))};
