#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "USubUVComponent.h"

#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UTexture.h"

#include "Asset/Pipeline/UPipeline.h"

#include "World/AActor.h"
#include "World/UWorld.h"
#include "World/Subsystem/URenderSubsystem.h"

void USubUVComponent::SetSubImage(Int32 InHorizontal, Int32 InVertical, Int32 InTotalFrame, float InFrameRate, bool BInLooping) {
    mSubImageHorizontal = InHorizontal;
    mSubImageVertical = InVertical;
    mTotalFrame = InTotalFrame;
    mFrameRate = InFrameRate;
    mBLooping = BInLooping;
}

void USubUVComponent::SetFrameRate(float InFrameRate) {
    mFrameRate = InFrameRate;
}

void USubUVComponent::SetCurrentFrame(Int32 InFrame) {
    mCurrentFrameIndex = InFrame;
}

void USubUVComponent::PlaySubUV() {
    mBPlaying = true;
}

void USubUVComponent::PauseSubUV() {
    mBPlaying = false;
}

void USubUVComponent::StopSubUV() {
    mBPlaying = false;
    SetCurrentFrame(0);
}

void USubUVComponent::RestartSubUV() {
    mElapsedTime = 0.0f;
    mCurrentFrameIndex = 0;
    mBPlaying = true;
}

bool USubUVComponent::IsPlaying() const {
    return mBPlaying;
}

bool USubUVComponent::IsLooping() const {
    return mBLooping;
}

void USubUVComponent::UpdateUVFromCurrentFrame() {
    if (mSubImageHorizontal <= 0 || mSubImageVertical <= 0) {
        return;
    }

    const Int32 Col{mCurrentFrameIndex % mSubImageHorizontal};
    const Int32 Row{mCurrentFrameIndex / mSubImageHorizontal};

    const float UWidth{1.0f / static_cast<float>(mSubImageHorizontal)};
    const float VHeight{1.0f / static_cast<float>(mSubImageVertical)};

    FVector2 NewUVMin{FVector2{Col * UWidth, Row * VHeight}};
    FVector2 NewUVMax{FVector2{(Col + 1) * UWidth, (Row + 1) * VHeight}};
    UBillboardComponent::SetUV(NewUVMin, NewUVMax);
}

void USubUVComponent::Tick(float DeltaTime) {
    UBillboardComponent::Tick(DeltaTime);

    if (!mBPlaying || mTotalFrame <= 1 || mFrameRate <= 0.0f || DeltaTime <= 0.0f) {
        return;
    }

    mElapsedTime += DeltaTime;

    const Int32 TargetFrame{static_cast<Int32>(mElapsedTime * mFrameRate)};
    if (mBLooping) {
        mCurrentFrameIndex = TargetFrame % mTotalFrame;
    } else {
        mCurrentFrameIndex = std::min(TargetFrame, mTotalFrame - 1);
        if (TargetFrame >= mTotalFrame) {
            mBPlaying = false;
        }
    }

    UpdateUVFromCurrentFrame();
}

void USubUVComponent::Serialize(FArchive& Archive) {
    UBillboardComponent::Serialize(Archive);
    Archive.Serialize("TotalFrame", mTotalFrame);
    Archive.Serialize("SubImageHorizontal", mSubImageHorizontal);
    Archive.Serialize("SubImageVertical", mSubImageVertical);
    Archive.Serialize("FrameRate", mFrameRate);
    Archive.Serialize("bLooping", mBLooping);
    Archive.Serialize("bPlaying", mBPlaying);

    if (Archive.IsLoading()) {
        mElapsedTime = 0.0f;
        mCurrentFrameIndex = 0;
        UpdateUVFromCurrentFrame();
    }
}

void USubUVComponent::DrawPanels(IPropertyEditorContext* Context) {
    UBillboardComponent::DrawPanels(Context);

    Context->DrawFloat("FrameRate", mFrameRate, 1.0f, 0.0f, 240.0f, [this](float NewRate) {
        SetFrameRate(NewRate);
    });
    Context->DrawVector2("SubImage", FVector2{static_cast<float>(mSubImageHorizontal), static_cast<float>(mSubImageVertical)}, 1.0f, 1.0f, 100.0f, [this](const FVector2& NewValue) {
        SetSubImage(static_cast<Int32>(NewValue.mX), static_cast<Int32>(NewValue.mY), mTotalFrame, mFrameRate, mBLooping);
    });
}
