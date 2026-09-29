#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace Stat {
    enum class EMemoryTag {
        Unknown,
        UObject,
        Container,
        String,
        Message,
        Count
    };

    // 태그별 메모리 통계
    struct FTagStats {
        std::size_t mAllocatedBytes{0};
        std::size_t mActiveAllocationCount{0};
    };

    struct FMemoryStats {
        std::size_t mAllocatedBytes{0};
        std::size_t mPeakAllocatedBytes{0};
        std::size_t mActiveAllocationCount{0};
        std::size_t mTotalAllocationCount{0};
        std::size_t mTotalDeallocationCount{0};

        // EMemoryTag::Count 크기만큼의 태그별 통계 배열
        FTagStats mTagStats[static_cast<std::size_t>(EMemoryTag::Count)]{};
    };

    enum class ESystemStatStage : std::size_t {
        FrameSetup,
        PreviewRender,
        EditorUi,
        WorldUpdate,
        RenderPreparation,
        Geometry,
        EditorOverlays,
        UiRender,
        Present,
        Other,
        Count
    };

    struct FSystemStatSample {
        double mTotalMilliseconds{};
        double mExclusiveMilliseconds{};
        std::uint64_t mCallCount{};
    };

    struct FFrameStats {
        double mDeltaSeconds{};
        double mElapsedSeconds{};
        double mFramesPerSecond{};
        double mAverageFrameMilliseconds{};
        std::uint64_t mFrameCount{};
    };

    struct FSystemStats {
        std::array<FSystemStatSample, static_cast<std::size_t>(ESystemStatStage::Count)> mSamples{};
        std::uint64_t mFrameCount{};
    };

    struct FObjectStats {
        std::size_t mObjectCount{};
        std::size_t mActorCount{};
    };

    struct FPickingStats {
        double mLastMilliseconds{};
        double mLastBroadPhaseMilliseconds{};
        double mLastNarrowPhaseMilliseconds{};
        bool mHasPhaseTiming{};
        double mTotalMilliseconds{};
        std::uint64_t mAttemptCount{};
    };

    struct FStats {
        FFrameStats mFrame{};
        FSystemStats mSystem{};
        FMemoryStats mMemory{};
        FObjectStats mObjects{};
        FPickingStats mPicking{};
    };

    struct FSystemStatAverage {
        double mTotalMilliseconds{};
        double mExclusiveMilliseconds{};
        double mCallCount{};
    };

    struct FTagStatAverage {
        double mAllocatedBytes{};
        double mActiveAllocationCount{};
    };

    struct FMemoryStatAverage {
        double mAllocatedBytes{};
        double mActiveAllocationCount{};
        std::size_t mPeakAllocatedBytes{};
        std::size_t mTotalAllocationCount{};
        std::size_t mTotalDeallocationCount{};
        std::array<FTagStatAverage, static_cast<std::size_t>(EMemoryTag::Count)> mTagStats{};
    };

    struct FObjectStatAverage {
        double mObjectCount{};
        double mActorCount{};
    };

    struct FPickingStatAverage {
        double mAverageMilliseconds{};
        double mMillisecondsPerFrame{};
        double mAttemptsPerFrame{};
    };

    struct FStatAverages {
        FFrameStats mFrame{};
        std::array<FSystemStatAverage, static_cast<std::size_t>(ESystemStatStage::Count)> mSystemSamples{};
        FMemoryStatAverage mMemory{};
        FObjectStatAverage mObjects{};
        FPickingStatAverage mPicking{};
        std::uint64_t mFrameCount{};
    };

    void BeginFrame();
    void EndFrame();
    void ResetFrameStats();

    void RecordSystemTime(ESystemStatStage Stage, double Milliseconds);
    void RecordAllocation(std::size_t Size, EMemoryTag Tag);
    void RecordDeallocation(std::size_t Size, EMemoryTag Tag);
    void RecordObjectCounts(std::size_t ObjectCount, std::size_t ActorCount);
    // A negative narrow-phase value means the optional breakdown was not measured.
    void RecordPickingTime(double Milliseconds, double NarrowPhaseMilliseconds = -1.0);

    FStats GetStats();
    FStatAverages GetStatAverages();
    FFrameStats GetFrameStats();
    FSystemStats GetSystemStats();
    FSystemStatSample GetSystemSample(ESystemStatStage Stage);
    FMemoryStats GetMemoryStats();
    FObjectStats GetObjectStats();
    FPickingStats GetPickingStats();

    const char* GetSystemStageName(ESystemStatStage Stage);
    // 태그 이름을 문자열로 반환하는 헬퍼 함수
    const char* GetMemoryTagName(EMemoryTag Tag);

    class FScopedSystemStatTimer {
    public:
        explicit FScopedSystemStatTimer(ESystemStatStage Stage);
        ~FScopedSystemStatTimer();
        FScopedSystemStatTimer(const FScopedSystemStatTimer&) = delete;
        FScopedSystemStatTimer& operator=(const FScopedSystemStatTimer&) = delete;
        FScopedSystemStatTimer(FScopedSystemStatTimer&&) = delete;
        FScopedSystemStatTimer& operator=(FScopedSystemStatTimer&&) = delete;

    private:
        ESystemStatStage mStage{};
        ESystemStatStage mPreviousStage{};
        std::chrono::steady_clock::time_point mStartTime{};
        std::uint64_t mFrameId{};
        bool mActive{};
    };
}
