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
    { 0.12f, 0.22f },
    { 0.05f, 0.1f },
    { 0.0f, 0.03f },
};

inline constexpr Uint32 GLODCount{static_cast<Uint32>(sizeof(GLODSettings) / sizeof(GLODSettings[0]))};
static_assert(GLODCount <= 32, "Available LODs use a 32-bit mask.");

// 각 경계의 +/-8% 구간에서만 두 LOD를 디더링으로 교차 전환한다.
inline constexpr float GLODTransitionHalfWidth{0.08f};
// 바운딩 구의 화면 지름(픽셀). 작은 물체만 서서히 지운 뒤 CPU에서 제외한다.
inline constexpr float GLODCullPixels{0.75f};
inline constexpr float GLODFadePixels{1.5f};
