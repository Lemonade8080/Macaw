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
        double mFrameWindowSeconds{};
        double mPickingWindowMilliseconds{};
        std::uint64_t mPickingWindowCount{};
        std::uint64_t mFrameWindowCount{};
        std::uint64_t mActiveFrameId{};
        bool mFrameActive{};
        bool mPublishAverages{};
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
        State.mPublishAverages = false;
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
            State.mPublishAverages = true;
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
    AccumulateAverages(State);
    if (State.mPublishAverages) {
        PublishAverages(State);
    }
    State.mFrameActive = false;
}

void Stat::ResetFrameStats() {
    FStatState& State{GetStatState()};
    State.mCurrentSamples = {};
    State.mWindowTotals = {};
    State.mAverages = {};
    State.mPickingWindowMilliseconds = 0.0;
    State.mPickingWindowCount = 0;
    State.mPublishAverages = false;
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
        case ESystemStatStage::RenderBeginFrame:
            return "Frame setup";
        case ESystemStatStage::RenderFenceWait:
            return "Fence wait";
        case ESystemStatStage::RenderView:
            return "View render (inclusive)";
        case ESystemStatStage::RenderTarget:
            return "Target bind / clear";
        case ESystemStatStage::RenderMaterials:
            return "Material upload";
        case ESystemStatStage::RenderQueue:
            return "Render queue build";
        case ESystemStatStage::RenderViewUpload:
            return "View / light / model upload";
        case ESystemStatStage::RenderGeometry:
            return "Scene geometry";
        case ESystemStatStage::RenderSelectionOutline:
            return "Selection outline";
        case ESystemStatStage::RenderSceneGuides:
            return "Scene guides";
        case ESystemStatStage::RenderGizmo:
            return "Gizmo";
        case ESystemStatStage::RenderText:
            return "Text";
        case ESystemStatStage::RenderBillboard:
            return "Billboard";
        case ESystemStatStage::RenderOrientationAxis:
            return "Orientation axis";
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

void Stat::RecordPickingTime(double Milliseconds) {
    if (!std::isfinite(Milliseconds) || Milliseconds < 0.0) {
        return;
    }
    FStatState& State{GetStatState()};
    FPickingStats& Stats{State.mStats.mPicking};
    Stats.mLastMilliseconds = Milliseconds;
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

