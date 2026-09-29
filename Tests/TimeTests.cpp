#include "Core/Time/FFrameTimer.h"
#include "Core/Stat/Stat.h"
#include "Core/Memory/Memory.h"
#include "World/FWorldTime.h"
#include "World/UWorld.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

class FTimeTestActor : public AActor {
public:
    void Tick(float DeltaTime) override;
    float GetLastDeltaSeconds() const;
    int GetTickCount() const;

private:
    float mLastDeltaSeconds{};
    int mTickCount{};
};

void FTimeTestActor::Tick(float DeltaTime) {
    mLastDeltaSeconds = DeltaTime;
    ++mTickCount;
}

float FTimeTestActor::GetLastDeltaSeconds() const {
    return mLastDeltaSeconds;
}

int FTimeTestActor::GetTickCount() const {
    return mTickCount;
}

void TestFrameTime() {
    FFrameTimer Timer{};
    Timer.SetMaxDeltaSeconds(0.001);
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    Timer.Tick();
    assert(Timer.GetDeltaSeconds() >= 0.001);
    assert(Timer.GetUpdateDeltaSeconds() == 0.001);
    assert(Timer.GetElapsedSeconds() == Timer.GetDeltaSeconds());
    assert(Timer.GetFrameCount() == 1);
    const double FirstElapsed{Timer.GetElapsedSeconds()};
    Timer.Tick();
    assert(Timer.GetElapsedSeconds() >= FirstElapsed);
    assert(std::abs(Timer.GetElapsedSeconds() - FirstElapsed - Timer.GetDeltaSeconds()) < 1.0e-9);
    Timer.SetMaxDeltaSeconds(-1.0);
    Timer.SetMaxDeltaSeconds(std::numeric_limits<double>::quiet_NaN());
    Timer.SetMaxDeltaSeconds(std::numeric_limits<double>::infinity());
    assert(Timer.GetMaxDeltaSeconds() == 0.001);
    Timer.Reset();
    assert(Timer.GetDeltaSeconds() == 0.0);
    assert(Timer.GetElapsedSeconds() == 0.0);
    assert(Timer.GetFrameCount() == 0);
    assert(Timer.GetMaxDeltaSeconds() == 0.001);
}

void TestWorldTime() {
    FWorldTime Time{};
    Time.Tick(0.25);
    Time.SetTimeScale(2.0);
    Time.Tick(0.25);
    assert(Time.GetDeltaSeconds() == 0.5);
    assert(Time.GetElapsedSeconds() == 0.75);
    Time.SetPaused(true);
    Time.Tick(10.0);
    assert(Time.GetDeltaSeconds() == 0.0);
    assert(Time.GetElapsedSeconds() == 0.75);
    Time.SetPaused(false);
    Time.Tick(0.125);
    assert(Time.GetElapsedSeconds() == 1.0);
    Time.SetTimeScale(0.0);
    Time.Tick(1.0);
    assert(Time.GetDeltaSeconds() == 0.0);
    assert(Time.GetElapsedSeconds() == 1.0);
    Time.SetTimeScale(-1.0);
    Time.SetTimeScale(std::numeric_limits<double>::quiet_NaN());
    assert(Time.GetTimeScale() == 0.0);
    Time.SetTimeScale(1.0);
    Time.Tick(-1.0);
    Time.Tick(std::numeric_limits<double>::infinity());
    Time.Tick(std::numeric_limits<double>::quiet_NaN());
    assert(Time.GetElapsedSeconds() == 1.0);
    Time.Reset();
    assert(Time.GetElapsedSeconds() == 0.0);
    assert(Time.GetTimeScale() == 1.0);
    assert(!Time.IsPaused());
}

void TestWorldIntegration() {
    UWorld World{};
    FTimeTestActor* Actor{static_cast<FTimeTestActor*>(World.AddActor(std::make_unique<FTimeTestActor>()))};
    assert(Actor != nullptr);
    World.GetTime().SetTimeScale(0.5);
    World.Tick(0.25f);
    assert(Actor->GetLastDeltaSeconds() == 0.125f);
    assert(Actor->GetTickCount() == 1);
    World.GetTime().SetPaused(true);
    World.Tick(10.0f);
    assert(Actor->GetTickCount() == 1);
    assert(World.GetTime().GetElapsedSeconds() == 0.125);
    World.GetTime().SetPaused(false);
    World.Tick(0.25f);
    assert(Actor->GetTickCount() == 2);
    assert(World.GetTime().GetElapsedSeconds() == 0.25);
    World.GetTime().SetTimeScale(0.0);
    World.Tick(0.25f);
    assert(Actor->GetTickCount() == 2);
    World.GetTime().SetPaused(true);
    assert(World.DestroyActor(Actor));
    World.Tick(0.25f);
    assert(World.GetActors().empty());
}

void TestStatSnapshots() {
    Stat::ResetFrameStats();
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 100.0);
    Stat::BeginFrame();
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 1.25);
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 2.5);
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, -1.0);
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, std::numeric_limits<double>::quiet_NaN());
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, std::numeric_limits<double>::infinity());
    Stat::RecordSystemTime(Stat::ESystemStatStage::Count, 1.0);
    Stat::BeginFrame();
    assert(Stat::GetFrameStats().mFrameCount == 0);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::RenderPreparation).mCallCount == 0);
    Stat::EndFrame();
    Stat::EndFrame();
    Stat::BeginFrame();
    Stat::FStats Snapshot{Stat::GetStats()};
    assert(Snapshot.mSystem.mFrameCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::RenderPreparation).mTotalMilliseconds == 3.75);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::RenderPreparation).mCallCount == 2);
    Snapshot.mSystem.mSamples[static_cast<std::size_t>(Stat::ESystemStatStage::RenderPreparation)].mTotalMilliseconds = 500.0;
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::RenderPreparation).mTotalMilliseconds == 3.75);
    Stat::BeginFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::RenderPreparation).mTotalMilliseconds == 3.75);
    Stat::EndFrame();
    Stat::BeginFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::RenderPreparation).mCallCount == 0);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Count).mCallCount == 0);
    assert(Snapshot.mSystem.mFrameCount == 1);
}

void MeasureEarlyReturn() {
    const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::EditorUi};
    return;
}

void TestStatScopes() {
    Stat::ResetFrameStats();
    {
        const Stat::FScopedSystemStatTimer InactiveStat{Stat::ESystemStatStage::Present};
        Stat::BeginFrame();
    }
    {
        const Stat::FScopedSystemStatTimer FrameStat{Stat::ESystemStatStage::Other};
        MeasureEarlyReturn();
        try {
            const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::WorldUpdate};
            throw std::runtime_error{"Test"};
        } catch (const std::runtime_error&) {
        }
    }
    Stat::EndFrame();
    Stat::BeginFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Present).mCallCount == 0);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::EditorUi).mCallCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::WorldUpdate).mCallCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Other).mCallCount == 2);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Other).mTotalMilliseconds >= Stat::GetSystemSample(Stat::ESystemStatStage::WorldUpdate).mTotalMilliseconds);
    Stat::BeginFrame();
    {
        const Stat::FScopedSystemStatTimer StaleStat{Stat::ESystemStatStage::WorldUpdate};
        Stat::ResetFrameStats();
        Stat::BeginFrame();
    }
    Stat::EndFrame();
    Stat::BeginFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::WorldUpdate).mCallCount == 0);
    assert(Stat::GetSystemStats().mFrameCount == 1);
    Stat::BeginFrame();
    {
        const Stat::FScopedSystemStatTimer StaleStat{Stat::ESystemStatStage::WorldUpdate};
        Stat::EndFrame();
        Stat::BeginFrame();
    }
    Stat::EndFrame();
    Stat::BeginFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::WorldUpdate).mCallCount == 0);
}

void TestFrameAndObjectStats() {
    Stat::ResetFrameStats();
    Stat::BeginFrame();
    assert(Stat::GetStatAverages().mFrameCount == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
    Stat::EndFrame();
    assert(Stat::GetStatAverages().mFrameCount == 0);
    Stat::BeginFrame();
    const double FirstElapsed{ Stat::GetFrameStats().mElapsedSeconds };
    assert(FirstElapsed > 0.0);
    std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
    Stat::EndFrame();
    Stat::BeginFrame();
    const Stat::FFrameStats Frame{ Stat::GetFrameStats() };
    assert(Frame.mDeltaSeconds > 0.0);
    assert(Frame.mElapsedSeconds > FirstElapsed);
    assert(Frame.mFrameCount == 2);
    assert(std::abs(Frame.mAverageFrameMilliseconds * Frame.mFramesPerSecond - 1000.0) < 1.0e-9);
    Stat::RecordObjectCounts(12, 3);
    const Stat::FStats Snapshot{ Stat::GetStats() };
    Stat::RecordObjectCounts(10, 2);
    assert(Snapshot.mObjects.mObjectCount == 12);
    assert(Snapshot.mObjects.mActorCount == 3);
    assert(Stat::GetObjectStats().mObjectCount == 10);
    assert(Stat::GetObjectStats().mActorCount == 2);
}

void TestCentralMemoryStats() {
    const Stat::FMemoryStats Before{Stat::GetMemoryStats()};
    void* Allocation{Memory::Allocate(128, 64, Memory::EMemoryTag::Message)};
    assert(reinterpret_cast<std::uintptr_t>(Allocation) % 64 == 0);
    const Stat::FMemoryStats Allocated{Stat::GetMemoryStats()};
    const std::size_t TagIndex{static_cast<std::size_t>(Stat::EMemoryTag::Message)};
    assert(Allocated.mAllocatedBytes == Before.mAllocatedBytes + 128);
    assert(Allocated.mActiveAllocationCount == Before.mActiveAllocationCount + 1);
    assert(Allocated.mTotalAllocationCount == Before.mTotalAllocationCount + 1);
    assert(Allocated.mTagStats[TagIndex].mAllocatedBytes == Before.mTagStats[TagIndex].mAllocatedBytes + 128);
    assert(Allocated.mPeakAllocatedBytes >= Allocated.mAllocatedBytes);
    Stat::ResetFrameStats();
    assert(Stat::GetMemoryStats().mAllocatedBytes == Allocated.mAllocatedBytes);
    assert(Memory::GetStats().mAllocatedBytes == Allocated.mAllocatedBytes);
    Memory::Free(Allocation);
    Memory::Free(nullptr);
    const Stat::FStats After{Stat::GetStats()};
    assert(After.mMemory.mAllocatedBytes == Before.mAllocatedBytes);
    assert(After.mMemory.mActiveAllocationCount == Before.mActiveAllocationCount);
    assert(After.mMemory.mTotalDeallocationCount == Before.mTotalDeallocationCount + 1);
    assert(After.mMemory.mTagStats[TagIndex].mAllocatedBytes == Before.mTagStats[TagIndex].mAllocatedBytes);
    assert(Allocated.mAllocatedBytes == Before.mAllocatedBytes + 128);
}

void TestStatAverages() {
    Stat::ResetFrameStats();
    const Stat::FMemoryStats Before{Stat::GetMemoryStats()};
    const std::size_t SceneIndex{static_cast<std::size_t>(Stat::ESystemStatStage::RenderPreparation)};
    const std::size_t GeometryIndex{static_cast<std::size_t>(Stat::ESystemStatStage::Geometry)};
    const std::size_t TagIndex{static_cast<std::size_t>(Stat::EMemoryTag::Message)};
    assert(Stat::GetStatAverages().mFrameCount == 0);
    Stat::BeginFrame();
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 8.0);
    Stat::RecordObjectCounts(10, 2);
    Stat::RecordAllocation(100, Stat::EMemoryTag::Message);
    Stat::RecordPickingTime(2.0);
    assert(Stat::GetStatAverages().mFrameCount == 0);
    Stat::EndFrame();
    assert(Stat::GetStatAverages().mFrameCount == 0);
    Stat::BeginFrame();
    const Stat::FStatAverages First{Stat::GetStatAverages()};
    assert(First.mFrameCount == 1);
    assert(First.mSystemSamples[SceneIndex].mTotalMilliseconds == 8.0);
    assert(First.mSystemSamples[SceneIndex].mCallCount == 1.0);
    assert(First.mFrame.mFramesPerSecond > 0.0);
    assert(First.mPicking.mAverageMilliseconds == 2.0);
    Stat::BeginFrame();
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 2.0);
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 4.0);
    Stat::RecordSystemTime(Stat::ESystemStatStage::Geometry, 8.0);
    Stat::RecordObjectCounts(20, 3);
    Stat::RecordAllocation(200, Stat::EMemoryTag::Message);
    Stat::RecordPickingTime(2.0);
    Stat::RecordPickingTime(6.0);
    Stat::RecordPickingTime(-1.0);
    Stat::RecordPickingTime(std::numeric_limits<double>::quiet_NaN());
    Stat::RecordPickingTime(std::numeric_limits<double>::infinity());
    Stat::EndFrame();
    assert(Stat::GetStatAverages().mSystemSamples[SceneIndex].mTotalMilliseconds == 8.0);
    assert(Stat::GetStatAverages().mObjects.mObjectCount == 10.0);
    assert(Stat::GetStatAverages().mMemory.mAllocatedBytes == static_cast<double>(Before.mAllocatedBytes) + 100.0);
    assert(Stat::GetStatAverages().mPicking.mAverageMilliseconds == 2.0);
    Stat::BeginFrame();
    Stat::RecordObjectCounts(30, 4);
    Stat::RecordDeallocation(200, Stat::EMemoryTag::Message);
    assert(Stat::GetStatAverages().mFrame.mFramesPerSecond == First.mFrame.mFramesPerSecond);
    std::this_thread::sleep_for(std::chrono::milliseconds{ 550 });
    Stat::EndFrame();
    Stat::EndFrame();
    Stat::BeginFrame();
    const Stat::FStatAverages Average{Stat::GetStatAverages()};
    assert(Average.mFrameCount == 2);
    assert(Average.mFrame.mFramesPerSecond > 0.0);
    assert(std::abs(Average.mFrame.mAverageFrameMilliseconds * Average.mFrame.mFramesPerSecond - 1000.0) < 1.0e-9);
    assert(Average.mSystemSamples[SceneIndex].mTotalMilliseconds == 3.0);
    assert(Average.mSystemSamples[SceneIndex].mCallCount == 1.0);
    assert(Average.mSystemSamples[GeometryIndex].mTotalMilliseconds == 4.0);
    assert(Average.mSystemSamples[GeometryIndex].mCallCount == 0.5);
    assert(Average.mObjects.mObjectCount == 25.0);
    assert(Average.mObjects.mActorCount == 3.5);
    assert(Average.mMemory.mAllocatedBytes == static_cast<double>(Before.mAllocatedBytes) + 200.0);
    assert(Average.mMemory.mActiveAllocationCount == static_cast<double>(Before.mActiveAllocationCount) + 1.5);
    assert(Average.mMemory.mTagStats[TagIndex].mAllocatedBytes == static_cast<double>(Before.mTagStats[TagIndex].mAllocatedBytes) + 200.0);
    assert(Average.mMemory.mTagStats[TagIndex].mActiveAllocationCount == static_cast<double>(Before.mTagStats[TagIndex].mActiveAllocationCount) + 1.5);
    assert(Average.mMemory.mPeakAllocatedBytes == Stat::GetMemoryStats().mPeakAllocatedBytes);
    assert(Average.mMemory.mTotalAllocationCount == Before.mTotalAllocationCount + 2);
    assert(Average.mMemory.mTotalDeallocationCount == Before.mTotalDeallocationCount + 1);
    assert(Average.mPicking.mAverageMilliseconds == 4.0);
    assert(Average.mPicking.mMillisecondsPerFrame == 4.0);
    assert(Average.mPicking.mAttemptsPerFrame == 1.0);
    assert(Stat::GetPickingStats().mAttemptCount == 3);
    assert(Stat::GetPickingStats().mTotalMilliseconds == 10.0);
    Stat::BeginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 550 });
    Stat::EndFrame();
    Stat::BeginFrame();
    const Stat::FStatAverages Empty{Stat::GetStatAverages()};
    assert(Empty.mFrameCount == 1);
    assert(Empty.mSystemSamples[SceneIndex].mTotalMilliseconds == 0.0);
    assert(Empty.mSystemSamples[GeometryIndex].mCallCount == 0.0);
    assert(Empty.mPicking.mAverageMilliseconds == 0.0);
    assert(Empty.mPicking.mMillisecondsPerFrame == 0.0);
    assert(Empty.mPicking.mAttemptsPerFrame == 0.0);
    assert(Empty.mObjects.mObjectCount == 30.0);
    Stat::BeginFrame();
    Stat::RecordSystemTime(Stat::ESystemStatStage::RenderPreparation, 100.0);
    Stat::RecordPickingTime(100.0);
    Stat::EndFrame();
    Stat::ResetFrameStats();
    assert(Stat::GetStatAverages().mFrameCount == 0);
    Stat::BeginFrame();
    Stat::EndFrame();
    assert(Stat::GetStatAverages().mSystemSamples[SceneIndex].mTotalMilliseconds == 0.0);
    assert(Stat::GetStatAverages().mPicking.mAverageMilliseconds == 0.0);
    Stat::RecordDeallocation(100, Stat::EMemoryTag::Message);
    assert(Stat::GetMemoryStats().mAllocatedBytes == Before.mAllocatedBytes);
}

void TestExclusiveFrameBreakdown() {
    Stat::ResetFrameStats();
    Stat::BeginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
    {
        const Stat::FScopedSystemStatTimer Setup{ Stat::ESystemStatStage::FrameSetup };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
    }
    {
        const Stat::FScopedSystemStatTimer Preview{ Stat::ESystemStatStage::PreviewRender };
        const Stat::FScopedSystemStatTimer Preparation{ Stat::ESystemStatStage::RenderPreparation };
        const Stat::FScopedSystemStatTimer Geometry{ Stat::ESystemStatStage::Geometry };
        const Stat::FScopedSystemStatTimer Overlays{ Stat::ESystemStatStage::EditorOverlays };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 3 });
    }
    {
        const Stat::FScopedSystemStatTimer Ui{ Stat::ESystemStatStage::EditorUi };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    {
        const Stat::FScopedSystemStatTimer Outer{ Stat::ESystemStatStage::WorldUpdate };
        const Stat::FScopedSystemStatTimer Inner{ Stat::ESystemStatStage::WorldUpdate };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
    }
    for (int Index{}; Index < 3; ++Index) {
        const Stat::FScopedSystemStatTimer Preparation{ Stat::ESystemStatStage::RenderPreparation };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
        {
            const Stat::FScopedSystemStatTimer Geometry{ Stat::ESystemStatStage::Geometry };
            std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
        }
        {
            const Stat::FScopedSystemStatTimer Overlays{ Stat::ESystemStatStage::EditorOverlays };
            std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
        }
    }
    {
        const Stat::FScopedSystemStatTimer Ui{ Stat::ESystemStatStage::UiRender };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    {
        const Stat::FScopedSystemStatTimer Present{ Stat::ESystemStatStage::Present };
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    }
    Stat::EndFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds{ 4 });
    Stat::BeginFrame();
    const Stat::FStatAverages Snapshot{ Stat::GetStatAverages() };
    double TotalMilliseconds{};
    double RoundedMicroseconds{};
    double RoundedShareTenths{};
    double PreviousMicroseconds{};
    double PreviousShareTenths{};
    for (const Stat::FSystemStatAverage& Sample : Snapshot.mSystemSamples) {
        assert(Sample.mExclusiveMilliseconds > 0.0);
        TotalMilliseconds += Sample.mExclusiveMilliseconds;
        const double CumulativeMicroseconds{ std::round(TotalMilliseconds * 1000.0) };
        const double CumulativeShareTenths{ std::round(TotalMilliseconds * 1000.0 / Snapshot.mFrame.mAverageFrameMilliseconds) };
        RoundedMicroseconds += CumulativeMicroseconds - PreviousMicroseconds;
        RoundedShareTenths += CumulativeShareTenths - PreviousShareTenths;
        PreviousMicroseconds = CumulativeMicroseconds;
        PreviousShareTenths = CumulativeShareTenths;
    }
    assert(std::abs(TotalMilliseconds - Snapshot.mFrame.mAverageFrameMilliseconds) < 1.0e-9);
    assert(RoundedMicroseconds == std::round(Snapshot.mFrame.mAverageFrameMilliseconds * 1000.0));
    assert(RoundedShareTenths == 1000.0);
    const Stat::FSystemStatAverage& Preview{ Snapshot.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::PreviewRender)] };
    assert(Preview.mExclusiveMilliseconds >= 3.0);
    assert(Preview.mExclusiveMilliseconds == Preview.mTotalMilliseconds);
    assert(Preview.mCallCount == 1.0);
    assert(Snapshot.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::Other)].mExclusiveMilliseconds >= 6.0);
    assert(Snapshot.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::RenderPreparation)].mCallCount == 3.0);
    assert(Snapshot.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::Geometry)].mCallCount == 3.0);
    assert(Snapshot.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::EditorOverlays)].mCallCount == 3.0);
    assert(Snapshot.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::WorldUpdate)].mCallCount == 2.0);
    Stat::ResetFrameStats();
    Stat::BeginFrame();
    Stat::EndFrame();
    Stat::BeginFrame();
    const Stat::FStatAverages Empty{ Stat::GetStatAverages() };
    double EmptyTotal{};
    for (const Stat::FSystemStatAverage& Sample : Empty.mSystemSamples) {
        EmptyTotal += Sample.mExclusiveMilliseconds;
    }
    assert(std::abs(EmptyTotal - Empty.mFrame.mAverageFrameMilliseconds) < 1.0e-9);
    assert(Empty.mSystemSamples[static_cast<std::size_t>(Stat::ESystemStatStage::WorldUpdate)].mCallCount == 0.0);
}
int main() {
    TestFrameTime();
    TestWorldTime();
    TestWorldIntegration();
    TestStatSnapshots();
    TestStatScopes();
    TestFrameAndObjectStats();
    TestCentralMemoryStats();
    TestStatAverages();
    TestExclusiveFrameBreakdown();
    std::cout << "All time and stat tests passed.\n";
    return 0;
}
