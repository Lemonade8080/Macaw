#include "pch.h"
#include "FWorldTime.h"

#include <cmath>
#include <limits>

void FWorldTime::Reset() {
    mDeltaSeconds = 0.0;
    mElapsedSeconds = 0.0;
    mTimeScale = 1.0;
    mPaused = false;
}

void FWorldTime::Tick(double DeltaSeconds) {
    mDeltaSeconds = 0.0;
    if (mPaused || !std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0) {
        return;
    }
    const double ScaledDeltaSeconds{DeltaSeconds * mTimeScale};
    if (!std::isfinite(ScaledDeltaSeconds) || ScaledDeltaSeconds > std::numeric_limits<float>::max() || !std::isfinite(mElapsedSeconds + ScaledDeltaSeconds)) {
        return;
    }
    mDeltaSeconds = ScaledDeltaSeconds;
    mElapsedSeconds += mDeltaSeconds;
}

void FWorldTime::SetPaused(bool Paused) {
    mPaused = Paused;
    if (mPaused) {
        mDeltaSeconds = 0.0;
    }
}

bool FWorldTime::IsPaused() const {
    return mPaused;
}

void FWorldTime::SetTimeScale(double TimeScale) {
    if (std::isfinite(TimeScale) && TimeScale >= 0.0) {
        mTimeScale = TimeScale;
    }
}

double FWorldTime::GetTimeScale() const {
    return mTimeScale;
}

double FWorldTime::GetDeltaSeconds() const {
    return mDeltaSeconds;
}

double FWorldTime::GetElapsedSeconds() const {
    return mElapsedSeconds;
}
