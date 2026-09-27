#pragma once

class FWorldTime {
public:
    void Reset();
    void Tick(double DeltaSeconds);

    void SetPaused(bool Paused);
    bool IsPaused() const;
    void SetTimeScale(double TimeScale);
    double GetTimeScale() const;

    double GetDeltaSeconds() const;
    double GetElapsedSeconds() const;

private:
    double mDeltaSeconds{};
    double mElapsedSeconds{};
    double mTimeScale{1.0};
    bool mPaused{};
};
