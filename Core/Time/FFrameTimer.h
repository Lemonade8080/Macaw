#pragma once

#include <chrono>
#include <cstdint>

class FFrameTimer {
public:
    FFrameTimer();

public:
    void Reset();
    void Tick();

    double GetDeltaSeconds() const;
    double GetUpdateDeltaSeconds() const;
    double GetElapsedSeconds() const;
    std::uint64_t GetFrameCount() const;

    void SetMaxDeltaSeconds(double MaxDeltaSeconds);
    double GetMaxDeltaSeconds() const;

private:
    std::chrono::steady_clock::time_point mStartTime{};
    std::chrono::steady_clock::time_point mLastTickTime{};
    double mDeltaSeconds{};
    double mElapsedSeconds{};
    double mMaxDeltaSeconds{0.1};
    std::uint64_t mFrameCount{};
};
