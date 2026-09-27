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
        Frame,
        Thumbnails,
        Offscreen,
        EditorUi,
        Input,
        WorldCommands,
        WorldTick,
        EditorDispatch,
        SceneRender,
        UiRender,
        Present,
        Count
    };

    struct FSystemStatSample {
        double mTotalMilliseconds{};
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

    struct FStats {
        FFrameStats mFrame{};
        FSystemStats mSystem{};
        FMemoryStats mMemory{};
        FObjectStats mObjects{};
    };

    void BeginFrame(double DeltaSeconds = 0.0);
    void EndFrame();
    void ResetFrameStats();

    void RecordSystemTime(ESystemStatStage Stage, double Milliseconds);
    void RecordAllocation(std::size_t Size, EMemoryTag Tag);
    void RecordDeallocation(std::size_t Size, EMemoryTag Tag);
    void RecordObjectCounts(std::size_t ObjectCount, std::size_t ActorCount);

    FStats GetStats();
    FFrameStats GetFrameStats();
    FSystemStats GetSystemStats();
    FSystemStatSample GetSystemSample(ESystemStatStage Stage);
    FMemoryStats GetMemoryStats();
    FObjectStats GetObjectStats();

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
        std::chrono::steady_clock::time_point mStartTime{};
        std::uint64_t mFrameId{};
        bool mActive{};
    };
}
