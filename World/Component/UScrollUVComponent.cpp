#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UScrollUVComponent.h"

#include "Core/Archive/FArchive.h"

void UScrollUVComponent::Tick(float DeltaTime) {
    UBillboardComponent::Tick(DeltaTime);

    if (!mBPlaying) {
        return;
    }

    mCurrentOffset += mScrollSpeed * DeltaTime;

    if (mBLooping) {

        mCurrentOffset.mX -= std::floor(mCurrentOffset.mX);
        mCurrentOffset.mY -= std::floor(mCurrentOffset.mY);
    }

    UpdateUVFromCurrentFrame();
}

void UScrollUVComponent::UpdateUVFromCurrentFrame() {

    UBillboardComponent::SetUV(mCurrentOffset, mCurrentOffset + FVector2{1.0f, 1.0f});
}

void UScrollUVComponent::Serialize(FArchive& Archive) {
    UBillboardComponent::Serialize(Archive);

    Archive.Serialize("ScrollSpeed", mScrollSpeed);
    Archive.Serialize("bLooping", mBLooping);
    Archive.Serialize("bPlaying", mBPlaying);

    if (Archive.IsLoading()) {
        mCurrentOffset = FVector2{0.0f, 0.0f};
        UpdateUVFromCurrentFrame();
    }
}

bool UScrollUVComponent::IsPlaying() const {
    return mBPlaying;
}

bool UScrollUVComponent::IsLooping() const {
    return mBLooping;
}

void UScrollUVComponent::PlayScrollUV() {
    mBPlaying = true;
}

void UScrollUVComponent::PauseScrollUV() {
    mBPlaying = false;
}

void UScrollUVComponent::SetScrollSpeed(FVector2 InScrollSpeed) {
    mScrollSpeed.mX = InScrollSpeed.mX, mScrollSpeed.mY = InScrollSpeed.mY;
}

void UScrollUVComponent::DrawPanels(IPropertyEditorContext* Context) {
    UBillboardComponent::DrawPanels(Context);

    Context->DrawVector2("ScrollSpeed", mScrollSpeed, 0.01f, -5.0f, 5.0f, [this](FVector2 NewSpeed) {
        SetScrollSpeed(NewSpeed);
    });
}
