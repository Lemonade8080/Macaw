#include "pch.h"
#include "Stat.h"

#include <algorithm>
#include <cmath>

using namespace Stat;

namespace {
    struct FStatState {
        std::array<FSystemStatSample, static_cast<std::size_t>(ESystemStatStage::Count)> mCurrentSamples{};
        FStats mStats{};
        double mFrameWindowSeconds{};
        std::uint64_t mFrameWindowCount{};
        std::uint64_t mActiveFrameId{};
        bool mFrameActive{};
    };

    FStatState& GetStatState() {
        static FStatState State{};
        return State;
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

void Stat::BeginFrame(double DeltaSeconds) {
    FStatState& State{GetStatState()};
    if (State.mFrameActive) {
        return;
    }
    State.mCurrentSamples = {};
    FFrameStats& Frame{State.mStats.mFrame};
    Frame.mDeltaSeconds = std::isfinite(DeltaSeconds) && DeltaSeconds > 0.0 ? DeltaSeconds : 0.0;
    Frame.mElapsedSeconds += Frame.mDeltaSeconds;
    ++Frame.mFrameCount;
    if (Frame.mDeltaSeconds > 0.0) {
        State.mFrameWindowSeconds += Frame.mDeltaSeconds;
        ++State.mFrameWindowCount;
        if (Frame.mFramesPerSecond == 0.0 || State.mFrameWindowSeconds >= 0.5) {
            Frame.mFramesPerSecond = static_cast<double>(State.mFrameWindowCount) / State.mFrameWindowSeconds;
            Frame.mAverageFrameMilliseconds = State.mFrameWindowSeconds * 1000.0 / static_cast<double>(State.mFrameWindowCount);
            State.mFrameWindowSeconds = 0.0;
            State.mFrameWindowCount = 0;
        }
    }
    ++State.mActiveFrameId;
    State.mFrameActive = true;
}

void Stat::EndFrame() {
    FStatState& State{GetStatState()};
    if (!State.mFrameActive) {
        return;
    }
    State.mStats.mSystem.mSamples = State.mCurrentSamples;
    ++State.mStats.mSystem.mFrameCount;
    State.mFrameActive = false;
}

void Stat::ResetFrameStats() {
    FStatState& State{GetStatState()};
    State.mCurrentSamples = {};
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
        case ESystemStatStage::Frame:
            return "Frame (inclusive)";
        case ESystemStatStage::Thumbnails:
            return "Thumbnails";
        case ESystemStatStage::Offscreen:
            return "Offscreen previews";
        case ESystemStatStage::EditorUi:
            return "Editor UI";
        case ESystemStatStage::Input:
            return "Editor input";
        case ESystemStatStage::WorldCommands:
            return "World commands";
        case ESystemStatStage::WorldTick:
            return "World tick";
        case ESystemStatStage::EditorDispatch:
            return "Editor dispatch";
        case ESystemStatStage::SceneRender:
            return "Scene render";
        case ESystemStatStage::UiRender:
            return "UI render";
        case ESystemStatStage::Present:
            return "Present / wait";
        default:
            return "Unknown";
    }
}

Stat::FScopedSystemStatTimer::FScopedSystemStatTimer(ESystemStatStage Stage)
	: mStage{Stage},
	  mStartTime{std::chrono::steady_clock::now()},
	  mFrameId{GetStatState().mActiveFrameId},
	  mActive{GetStatState().mFrameActive} {
}

Stat::FScopedSystemStatTimer::~FScopedSystemStatTimer() {
    if (mActive) {
        const double Milliseconds{std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - mStartTime}.count()};
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
    FPickingStats& Stats{GetStatState().mStats.mPicking};
    Stats.mLastMilliseconds = Milliseconds;
    Stats.mHasPhaseTiming = std::isfinite(NarrowPhaseMilliseconds) && NarrowPhaseMilliseconds >= 0.0 && NarrowPhaseMilliseconds <= Milliseconds;
    Stats.mLastNarrowPhaseMilliseconds = Stats.mHasPhaseTiming ? NarrowPhaseMilliseconds : 0.0;
    Stats.mLastBroadPhaseMilliseconds = Stats.mHasPhaseTiming ? Milliseconds - NarrowPhaseMilliseconds : 0.0;
    Stats.mTotalMilliseconds += Milliseconds;
    ++Stats.mAttemptCount;
}

Stat::FStats Stat::GetStats() {
    return GetStatState().mStats;
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

