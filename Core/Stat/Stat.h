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

    enum class ERenderPreparationStage : std::size_t {
        SceneData,
        SceneSynchronization,
        ViewSetup,
        MaterialBuffer,
        RenderQueue,
        ViewBuffers,
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

    struct FWorldTickStats {
        double mTotalMilliseconds{};
        std::uint64_t mActorTickCount{};
        std::uint64_t mComponentVisitCount{};
        std::uint64_t mComponentTickCount{};
    };

    struct FPickingStats {
        double mLastMilliseconds{};
        double mTotalMilliseconds{};
        std::uint64_t mAttemptCount{};
    };

    struct FLODStats {
        std::uint32_t mLevel{};
        std::uint64_t mRenderedTriangleCount{};
        std::uint64_t mOriginalTriangleCount{};
        std::uint64_t mDrawCallCount{};
    };

    struct FStats {
        FFrameStats mFrame{};
        FSystemStats mSystem{};
        std::array<FSystemStatSample, static_cast<std::size_t>(ERenderPreparationStage::Count)> mRenderPreparationSamples{};
        FMemoryStats mMemory{};
        FObjectStats mObjects{};
        FWorldTickStats mWorldTick{};
        FPickingStats mPicking{};
        FLODStats mLOD{};
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

    struct FWorldTickStatAverage {
        double mTotalMilliseconds{};
        double mActorTickCount{};
        double mComponentVisitCount{};
        double mComponentTickCount{};
    };

    struct FPickingStatAverage {
        double mAverageMilliseconds{};
        double mMillisecondsPerFrame{};
        double mAttemptsPerFrame{};
    };

    struct FLODStatAverage {
        std::uint32_t mLevel{};
        double mRenderedTriangleCount{};
        double mOriginalTriangleCount{};
        double mDrawCallCount{};
    };

    struct FStatAverages {
        FFrameStats mFrame{};
        std::array<FSystemStatAverage, static_cast<std::size_t>(ESystemStatStage::Count)> mSystemSamples{};
        std::array<FSystemStatAverage, static_cast<std::size_t>(ERenderPreparationStage::Count)> mRenderPreparationSamples{};
        FMemoryStatAverage mMemory{};
        FObjectStatAverage mObjects{};
        FWorldTickStatAverage mWorldTick{};
        FPickingStatAverage mPicking{};
        FLODStatAverage mLOD{};
        std::uint64_t mFrameCount{};
    };

    void BeginFrame();
    void EndFrame();
    void ResetFrameStats();

    void RecordSystemTime(ESystemStatStage Stage, double Milliseconds);
    void RecordAllocation(std::size_t Size, EMemoryTag Tag);
    void RecordDeallocation(std::size_t Size, EMemoryTag Tag);
    void RecordObjectCounts(std::size_t ObjectCount, std::size_t ActorCount);
    void RecordPickingTime(double Milliseconds);
    void RecordLODStats(std::uint32_t Level, std::uint64_t RenderedTriangleCount, std::uint64_t OriginalTriangleCount, std::uint64_t DrawCallCount);

    FStats GetStats();
    FStatAverages GetStatAverages();
    FFrameStats GetFrameStats();
    FSystemStats GetSystemStats();
    FSystemStatSample GetSystemSample(ESystemStatStage Stage);
    FSystemStatSample GetRenderPreparationSample(ERenderPreparationStage Stage);
    FMemoryStats GetMemoryStats();
    FObjectStats GetObjectStats();
    FWorldTickStats GetWorldTickStats();
    FWorldTickStats* GetActiveWorldTickStats();
    FPickingStats GetPickingStats();
    FLODStats GetLODStats();

    const char* GetSystemStageName(ESystemStatStage Stage);
    const char* GetRenderPreparationStageName(ERenderPreparationStage Stage);
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

    class FScopedWorldTickStatTimer {
    public:
        explicit FScopedWorldTickStatTimer(std::size_t ActorTickCount);
        ~FScopedWorldTickStatTimer();
        FScopedWorldTickStatTimer(const FScopedWorldTickStatTimer&) = delete;
        FScopedWorldTickStatTimer& operator=(const FScopedWorldTickStatTimer&) = delete;
        FScopedWorldTickStatTimer(FScopedWorldTickStatTimer&&) = delete;
        FScopedWorldTickStatTimer& operator=(FScopedWorldTickStatTimer&&) = delete;

    private:
        FWorldTickStats mStats{};
        std::chrono::steady_clock::time_point mStartTime{};
        std::uint64_t mFrameId{};
        bool mActive{};
    };

    class FScopedRenderPreparationStatTimer {
    public:
        explicit FScopedRenderPreparationStatTimer(ERenderPreparationStage Stage);
        ~FScopedRenderPreparationStatTimer();
        FScopedRenderPreparationStatTimer(const FScopedRenderPreparationStatTimer&) = delete;
        FScopedRenderPreparationStatTimer& operator=(const FScopedRenderPreparationStatTimer&) = delete;
        FScopedRenderPreparationStatTimer(FScopedRenderPreparationStatTimer&&) = delete;
        FScopedRenderPreparationStatTimer& operator=(FScopedRenderPreparationStatTimer&&) = delete;

    private:
        ERenderPreparationStage mStage{};
        std::chrono::steady_clock::time_point mStartTime{};
        std::uint64_t mFrameId{};
        bool mActive{};
    };
}
