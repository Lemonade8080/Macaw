#include "pch.h"
#include "FFrameTimer.h"

#include <algorithm>
#include <cmath>

FFrameTimer::FFrameTimer() {
    Reset();
}

void FFrameTimer::Reset() {
    mStartTime = std::chrono::steady_clock::now();
    mLastTickTime = mStartTime;
    mDeltaSeconds = 0.0;
    mElapsedSeconds = 0.0;
    mFrameCount = 0;
}

void FFrameTimer::Tick() {
    const auto CurrentTime{std::chrono::steady_clock::now()};
    mDeltaSeconds = std::chrono::duration<double>{CurrentTime - mLastTickTime}.count();
    mElapsedSeconds = std::chrono::duration<double>{CurrentTime - mStartTime}.count();
    mLastTickTime = CurrentTime;
    ++mFrameCount;
}

double FFrameTimer::GetDeltaSeconds() const {
    return mDeltaSeconds;
}

double FFrameTimer::GetUpdateDeltaSeconds() const {
    return std::min(mDeltaSeconds, mMaxDeltaSeconds);
}

double FFrameTimer::GetElapsedSeconds() const {
    return mElapsedSeconds;
}

std::uint64_t FFrameTimer::GetFrameCount() const {
    return mFrameCount;
}

void FFrameTimer::SetMaxDeltaSeconds(double MaxDeltaSeconds) {
    if (std::isfinite(MaxDeltaSeconds) && MaxDeltaSeconds > 0.0) {
        mMaxDeltaSeconds = MaxDeltaSeconds;
    }
}

double FFrameTimer::GetMaxDeltaSeconds() const {
    return mMaxDeltaSeconds;
}
