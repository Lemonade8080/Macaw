#include "pch.h"
#include "Stat.h"

#include <algorithm>
#include <cmath>

using namespace Stat;

namespace {
    struct FStatState {
        std::array<FSystemStatSample, static_cast<std::size_t>(ESystemStatStage::Count)> mCurrentSamples{};
        FStats mStats{};
        FStatAverages mWindowTotals{};
        FStatAverages mAverages{};
        std::chrono::steady_clock::time_point mFrameStartTime{};
        std::chrono::steady_clock::time_point mLastSampleTime{};
        ESystemStatStage mCurrentStage{ ESystemStatStage::Other };
        double mFrameWindowSeconds{};
        double mPickingWindowMilliseconds{};
        std::uint64_t mPickingWindowCount{};
        std::uint64_t mFrameWindowCount{};
        std::uint64_t mActiveFrameId{};
        bool mFrameActive{};
        bool mFramePending{};
    };

    FStatState& GetStatState() {
        static FStatState State{};
        return State;
    }

    void AccumulateAverages(FStatState& State) {
        FStatAverages& Totals{State.mWindowTotals};
        ++Totals.mFrameCount;
        for (std::size_t Index{}; Index < Totals.mSystemSamples.size(); ++Index) {
            Totals.mSystemSamples[Index].mTotalMilliseconds += State.mCurrentSamples[Index].mTotalMilliseconds;
            Totals.mSystemSamples[Index].mExclusiveMilliseconds += State.mCurrentSamples[Index].mExclusiveMilliseconds;
            Totals.mSystemSamples[Index].mCallCount += static_cast<double>(State.mCurrentSamples[Index].mCallCount);
        }
        Totals.mObjects.mObjectCount += static_cast<double>(State.mStats.mObjects.mObjectCount);
        Totals.mObjects.mActorCount += static_cast<double>(State.mStats.mObjects.mActorCount);
        const FMemoryStats& Memory{State.mStats.mMemory};
        Totals.mMemory.mAllocatedBytes += static_cast<double>(Memory.mAllocatedBytes);
        Totals.mMemory.mActiveAllocationCount += static_cast<double>(Memory.mActiveAllocationCount);
        Totals.mMemory.mPeakAllocatedBytes = Memory.mPeakAllocatedBytes;
        Totals.mMemory.mTotalAllocationCount = Memory.mTotalAllocationCount;
        Totals.mMemory.mTotalDeallocationCount = Memory.mTotalDeallocationCount;
        for (std::size_t Index{}; Index < Totals.mMemory.mTagStats.size(); ++Index) {
            Totals.mMemory.mTagStats[Index].mAllocatedBytes += static_cast<double>(Memory.mTagStats[Index].mAllocatedBytes);
            Totals.mMemory.mTagStats[Index].mActiveAllocationCount += static_cast<double>(Memory.mTagStats[Index].mActiveAllocationCount);
        }
    }

    void PublishAverages(FStatState& State) {
        State.mAverages = State.mWindowTotals;
        FStatAverages& Averages{State.mAverages};
        const double FrameCount{static_cast<double>(Averages.mFrameCount)};
        Averages.mFrame = State.mStats.mFrame;
        for (FSystemStatAverage& Sample : Averages.mSystemSamples) {
            Sample.mTotalMilliseconds /= FrameCount;
            Sample.mExclusiveMilliseconds /= FrameCount;
            Sample.mCallCount /= FrameCount;
        }
        Averages.mObjects.mObjectCount /= FrameCount;
        Averages.mObjects.mActorCount /= FrameCount;
        Averages.mMemory.mAllocatedBytes /= FrameCount;
        Averages.mMemory.mActiveAllocationCount /= FrameCount;
        for (FTagStatAverage& Tag : Averages.mMemory.mTagStats) {
            Tag.mAllocatedBytes /= FrameCount;
            Tag.mActiveAllocationCount /= FrameCount;
        }
        Averages.mPicking.mAverageMilliseconds = State.mPickingWindowCount > 0 ? State.mPickingWindowMilliseconds / static_cast<double>(State.mPickingWindowCount) : 0.0;
        Averages.mPicking.mMillisecondsPerFrame = State.mPickingWindowMilliseconds / FrameCount;
        Averages.mPicking.mAttemptsPerFrame = static_cast<double>(State.mPickingWindowCount) / FrameCount;
        State.mWindowTotals = {};
        State.mPickingWindowMilliseconds = 0.0;
        State.mPickingWindowCount = 0;
    }

    void RecordExclusiveTime(FStatState& State, std::chrono::steady_clock::time_point CurrentTime) {
        State.mCurrentSamples[static_cast<std::size_t>(State.mCurrentStage)].mExclusiveMilliseconds += std::chrono::duration<double, std::milli>{ CurrentTime - State.mLastSampleTime }.count();
        State.mLastSampleTime = CurrentTime;
    }

    void RecordSample(ESystemStatStage Stage, double Milliseconds, std::uint64_t FrameId) {
        FStatState& State{GetStatState()};
        const std::size_t Index{static_cast<std::size_t>(Stage)};
        if (!State.mFrameActive || FrameId != State.mActiveFrameId || Index >= State.mCurrentSamples.size() || !std::isfinite(Milliseconds) || Milliseconds < 0.0) {
            return;
        }
        FSystemStatSample& Sample{State.mCurrentSamples[Index]};
        Sample.mTotalMilliseconds += Milliseconds;
        ++Sample.mCallCount;
    }
}

void Stat::BeginFrame() {
    FStatState& State{GetStatState()};
    if (State.mFrameActive) {
        return;
    }
    const auto CurrentTime{ std::chrono::steady_clock::now() };
    if (State.mFramePending) {
        FFrameStats& Frame{ State.mStats.mFrame };
        Frame.mDeltaSeconds = std::chrono::duration<double>{ CurrentTime - State.mFrameStartTime }.count();
        Frame.mElapsedSeconds += Frame.mDeltaSeconds;
        ++Frame.mFrameCount;
        FSystemStatSample& Other{ State.mCurrentSamples[static_cast<std::size_t>(ESystemStatStage::Other)] };
        const double GapMilliseconds{ std::chrono::duration<double, std::milli>{ CurrentTime - State.mLastSampleTime }.count() };
        Other.mTotalMilliseconds += GapMilliseconds;
        Other.mExclusiveMilliseconds += GapMilliseconds;
        ++Other.mCallCount;
        State.mStats.mSystem.mSamples = State.mCurrentSamples;
        ++State.mStats.mSystem.mFrameCount;
        AccumulateAverages(State);
        State.mFrameWindowSeconds += Frame.mDeltaSeconds;
        ++State.mFrameWindowCount;
        if (State.mFrameWindowSeconds > 0.0 && (Frame.mFramesPerSecond == 0.0 || State.mFrameWindowSeconds >= 0.5)) {
            Frame.mFramesPerSecond = static_cast<double>(State.mFrameWindowCount) / State.mFrameWindowSeconds;
            Frame.mAverageFrameMilliseconds = State.mFrameWindowSeconds * 1000.0 / static_cast<double>(State.mFrameWindowCount);
            PublishAverages(State);
            State.mFrameWindowSeconds = 0.0;
            State.mFrameWindowCount = 0;
        }
    }
    State.mCurrentSamples = {};
    State.mFrameStartTime = CurrentTime;
    State.mLastSampleTime = CurrentTime;
    State.mCurrentStage = ESystemStatStage::Other;
    State.mFramePending = false;
    ++State.mActiveFrameId;
    State.mFrameActive = true;
}

void Stat::EndFrame() {
    FStatState& State{GetStatState()};
    if (!State.mFrameActive) {
        return;
    }
    RecordExclusiveTime(State, std::chrono::steady_clock::now());
    State.mFrameActive = false;
    State.mFramePending = true;
}

void Stat::ResetFrameStats() {
    FStatState& State{GetStatState()};
    State.mCurrentSamples = {};
    State.mWindowTotals = {};
    State.mAverages = {};
    State.mPickingWindowMilliseconds = 0.0;
    State.mPickingWindowCount = 0;
    State.mFramePending = false;
    State.mFrameStartTime = {};
    State.mLastSampleTime = {};
    State.mCurrentStage = ESystemStatStage::Other;
    State.mStats.mSystem = {};
    State.mStats.mFrame = {};
    State.mStats.mPicking = {};
    State.mFrameWindowSeconds = 0.0;
    State.mFrameWindowCount = 0;
    ++State.mActiveFrameId;
    State.mFrameActive = false;
}

void Stat::RecordSystemTime(ESystemStatStage Stage, double Milliseconds) {
    RecordSample(Stage, Milliseconds, GetStatState().mActiveFrameId);
}

Stat::FSystemStats Stat::GetSystemStats() {
    return GetStatState().mStats.mSystem;
}

FSystemStatSample Stat::GetSystemSample(ESystemStatStage Stage) {
    const std::size_t Index{static_cast<std::size_t>(Stage)};
    const FSystemStats& Stats{GetStatState().mStats.mSystem};
    return Index < Stats.mSamples.size() ? Stats.mSamples[Index] : FSystemStatSample{};
}

const char* Stat::GetSystemStageName(ESystemStatStage Stage) {
    switch (Stage) {
        case ESystemStatStage::FrameSetup:
            return "Frame setup / fence wait";
        case ESystemStatStage::PreviewRender:
            return "Preview render";
        case ESystemStatStage::EditorUi:
            return "Editor UI";
        case ESystemStatStage::WorldUpdate:
            return "Input / world update";
        case ESystemStatStage::RenderPreparation:
            return "Scene / render preparation";
        case ESystemStatStage::Geometry:
            return "Scene geometry";
        case ESystemStatStage::EditorOverlays:
            return "Editor overlays";
        case ESystemStatStage::UiRender:
            return "UI render";
        case ESystemStatStage::Present:
            return "Present / wait";
        case ESystemStatStage::Other:
            return "Other work / between frames";
        default:
            return "Unknown";
    }
}

Stat::FScopedSystemStatTimer::FScopedSystemStatTimer(ESystemStatStage Stage)
	: mStage{Stage},
	  mPreviousStage{ GetStatState().mCurrentStage },
	  mStartTime{std::chrono::steady_clock::now()},
	  mFrameId{GetStatState().mActiveFrameId},
	  mActive{ GetStatState().mFrameActive && static_cast<std::size_t>(Stage) < static_cast<std::size_t>(ESystemStatStage::Count) && mPreviousStage != ESystemStatStage::PreviewRender } {
    if (mActive) {
        FStatState& State{ GetStatState() };
        RecordExclusiveTime(State, mStartTime);
        State.mCurrentStage = mStage;
    }
}

Stat::FScopedSystemStatTimer::~FScopedSystemStatTimer() {
    FStatState& State{ GetStatState() };
    if (mActive && State.mFrameActive && State.mActiveFrameId == mFrameId) {
        const auto CurrentTime{ std::chrono::steady_clock::now() };
        RecordExclusiveTime(State, CurrentTime);
        State.mCurrentStage = mPreviousStage;
        const double Milliseconds{ std::chrono::duration<double, std::milli>{ CurrentTime - mStartTime }.count() };
        RecordSample(mStage, Milliseconds, mFrameId);
    }
}

void Stat::RecordAllocation(std::size_t Size, EMemoryTag Tag) {
    FMemoryStats& Stats{GetStatState().mStats.mMemory};
    // 전체 메모리 통계 갱신
    Stats.mAllocatedBytes += Size;
    ++Stats.mActiveAllocationCount;
    ++Stats.mTotalAllocationCount;

    Stats.mPeakAllocatedBytes = (std::max)(Stats.mPeakAllocatedBytes, Stats.mAllocatedBytes);

    // 태그별 메모리 통계 갱신
    const std::size_t TagIndex{static_cast<std::size_t>(Tag)};
    if (TagIndex < static_cast<std::size_t>(EMemoryTag::Count)) {
        Stats.mTagStats[TagIndex].mAllocatedBytes += Size;
        ++Stats.mTagStats[TagIndex].mActiveAllocationCount;
    }

}

void Stat::RecordDeallocation(std::size_t Size, EMemoryTag Tag) {
    FMemoryStats& Stats{GetStatState().mStats.mMemory};
    // 전체 메모리 통계 감소
    Stats.mAllocatedBytes -= Size;
    --Stats.mActiveAllocationCount;
    ++Stats.mTotalDeallocationCount;

    // 태그별 메모리 통계 감소
    const std::size_t TagIndex{static_cast<std::size_t>(Tag)};
    if (TagIndex < static_cast<std::size_t>(EMemoryTag::Count)) {
        Stats.mTagStats[TagIndex].mAllocatedBytes -= Size;
        --Stats.mTagStats[TagIndex].mActiveAllocationCount;
    }

}

void Stat::RecordObjectCounts(std::size_t ObjectCount, std::size_t ActorCount) {
    GetStatState().mStats.mObjects = FObjectStats{ObjectCount, ActorCount};
}

void Stat::RecordPickingTime(double Milliseconds, double NarrowPhaseMilliseconds) {
    if (!std::isfinite(Milliseconds) || Milliseconds < 0.0) {
        return;
    }
    FStatState& State{GetStatState()};
    FPickingStats& Stats{State.mStats.mPicking};
    Stats.mLastMilliseconds = Milliseconds;
    Stats.mHasPhaseTiming = std::isfinite(NarrowPhaseMilliseconds) && NarrowPhaseMilliseconds >= 0.0 && NarrowPhaseMilliseconds <= Milliseconds;
    Stats.mLastNarrowPhaseMilliseconds = Stats.mHasPhaseTiming ? NarrowPhaseMilliseconds : 0.0;
    Stats.mLastBroadPhaseMilliseconds = Stats.mHasPhaseTiming ? Milliseconds - NarrowPhaseMilliseconds : 0.0;
    Stats.mTotalMilliseconds += Milliseconds;
    ++Stats.mAttemptCount;
    State.mPickingWindowMilliseconds += Milliseconds;
    ++State.mPickingWindowCount;
}

Stat::FStats Stat::GetStats() {
    return GetStatState().mStats;
}

FStatAverages Stat::GetStatAverages() {
    return GetStatState().mAverages;
}

Stat::FFrameStats Stat::GetFrameStats() {
    return GetStatState().mStats.mFrame;
}

Stat::FMemoryStats Stat::GetMemoryStats() {
    return GetStatState().mStats.mMemory;
}

Stat::FObjectStats Stat::GetObjectStats() {
    return GetStatState().mStats.mObjects;
}

Stat::FPickingStats Stat::GetPickingStats() {
    return GetStatState().mStats.mPicking;
}

const char* Stat::GetMemoryTagName(EMemoryTag Tag) {
    switch (Tag) {
        case EMemoryTag::Unknown:
            return "Unknown";
        case EMemoryTag::UObject:
            return "UObject";
        case EMemoryTag::Container:
            return "Container";
        case EMemoryTag::String:
            return "String";
        case EMemoryTag::Message:
            return "Message";
        default:
            return "Invalid";
    }
}

