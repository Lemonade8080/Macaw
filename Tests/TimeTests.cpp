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
    Stat::RecordSystemTime(Stat::ESystemStatStage::SceneRender, 100.0);
    Stat::BeginFrame();
    Stat::RecordSystemTime(Stat::ESystemStatStage::SceneRender, 1.25);
    Stat::RecordSystemTime(Stat::ESystemStatStage::SceneRender, 2.5);
    Stat::RecordSystemTime(Stat::ESystemStatStage::SceneRender, -1.0);
    Stat::RecordSystemTime(Stat::ESystemStatStage::SceneRender, std::numeric_limits<double>::quiet_NaN());
    Stat::RecordSystemTime(Stat::ESystemStatStage::SceneRender, std::numeric_limits<double>::infinity());
    Stat::RecordSystemTime(Stat::ESystemStatStage::Count, 1.0);
    Stat::BeginFrame();
    assert(Stat::GetFrameStats().mFrameCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::SceneRender).mCallCount == 0);
    Stat::EndFrame();
    Stat::EndFrame();
    Stat::FStats Snapshot{Stat::GetStats()};
    assert(Snapshot.mSystem.mFrameCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::SceneRender).mTotalMilliseconds == 3.75);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::SceneRender).mCallCount == 2);
    Snapshot.mSystem.mSamples[static_cast<std::size_t>(Stat::ESystemStatStage::SceneRender)].mTotalMilliseconds = 500.0;
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::SceneRender).mTotalMilliseconds == 3.75);
    Stat::BeginFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::SceneRender).mTotalMilliseconds == 3.75);
    Stat::EndFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::SceneRender).mCallCount == 0);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Count).mCallCount == 0);
    assert(Snapshot.mSystem.mFrameCount == 1);
}

void MeasureEarlyReturn() {
    const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::Input};
    return;
}

void TestStatScopes() {
    Stat::ResetFrameStats();
    {
        const Stat::FScopedSystemStatTimer InactiveStat{Stat::ESystemStatStage::Present};
        Stat::BeginFrame();
    }
    {
        const Stat::FScopedSystemStatTimer FrameStat{Stat::ESystemStatStage::Frame};
        MeasureEarlyReturn();
        try {
            const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::WorldTick};
            throw std::runtime_error{"Test"};
        } catch (const std::runtime_error&) {
        }
    }
    Stat::EndFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Present).mCallCount == 0);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Input).mCallCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::WorldTick).mCallCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Frame).mCallCount == 1);
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::Frame).mTotalMilliseconds >= Stat::GetSystemSample(Stat::ESystemStatStage::WorldTick).mTotalMilliseconds);
    Stat::BeginFrame();
    {
        const Stat::FScopedSystemStatTimer StaleStat{Stat::ESystemStatStage::WorldTick};
        Stat::ResetFrameStats();
        Stat::BeginFrame();
    }
    Stat::EndFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::WorldTick).mCallCount == 0);
    assert(Stat::GetSystemStats().mFrameCount == 1);
    Stat::BeginFrame();
    {
        const Stat::FScopedSystemStatTimer StaleStat{Stat::ESystemStatStage::WorldTick};
        Stat::EndFrame();
        Stat::BeginFrame();
    }
    Stat::EndFrame();
    assert(Stat::GetSystemSample(Stat::ESystemStatStage::WorldTick).mCallCount == 0);
}

void TestFrameAndObjectStats() {
    Stat::ResetFrameStats();
    Stat::BeginFrame(0.25);
    Stat::EndFrame();
    Stat::BeginFrame(0.5);
    Stat::EndFrame();
    const Stat::FFrameStats Frame{Stat::GetFrameStats()};
    assert(Frame.mDeltaSeconds == 0.5);
    assert(Frame.mElapsedSeconds == 0.75);
    assert(Frame.mFrameCount == 2);
    assert(Frame.mFramesPerSecond == 2.0);
    assert(Frame.mAverageFrameMilliseconds == 500.0);
    Stat::BeginFrame(std::numeric_limits<double>::quiet_NaN());
    Stat::EndFrame();
    assert(Stat::GetFrameStats().mDeltaSeconds == 0.0);
    assert(Stat::GetFrameStats().mElapsedSeconds == 0.75);
    Stat::RecordObjectCounts(12, 3);
    const Stat::FStats Snapshot{Stat::GetStats()};
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

int main() {
    TestFrameTime();
    TestWorldTime();
    TestWorldIntegration();
    TestStatSnapshots();
    TestStatScopes();
    TestFrameAndObjectStats();
    TestCentralMemoryStats();
    std::cout << "All time and stat tests passed.\n";
    return 0;
}
