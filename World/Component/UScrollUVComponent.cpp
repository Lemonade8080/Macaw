#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UScrollUVComponent.h"

#include "Core/Archive/FArchive.h"

UScrollUVComponent::UScrollUVComponent() {
    UpdateTickEnabled();
}

void UScrollUVComponent::UpdateTickEnabled() {
    SetTickEnabled(mBPlaying && (mScrollSpeed.mX != 0.0f || mScrollSpeed.mY != 0.0f));
}

void UScrollUVComponent::Tick(float DeltaTime) {
    UBillboardComponent::Tick(DeltaTime);

    if (!mBPlaying) {
        return;
    }

    mCurrentOffset += mScrollSpeed * DeltaTime;

    if (mBLooping) {

        // 누적값이 커지면 float 정밀도가 깨져 UV가 계단진다. 소수부만 남긴다.
        mCurrentOffset.mX -= std::floor(mCurrentOffset.mX);
        mCurrentOffset.mY -= std::floor(mCurrentOffset.mY);
    }

    UpdateUVFromCurrentFrame();
}

void UScrollUVComponent::UpdateUVFromCurrentFrame() {

    // UV 창 전체를 오프셋만큼 민다. 0~1 을 벗어나는 부분은 Wrap 샘플러가 처리한다.
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
        UpdateTickEnabled();
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
    UpdateTickEnabled();
}

void UScrollUVComponent::PauseScrollUV() {
    mBPlaying = false;
    UpdateTickEnabled();
}

void UScrollUVComponent::SetScrollSpeed(FVector2 InScrollSpeed) {
    mScrollSpeed.mX = InScrollSpeed.mX, mScrollSpeed.mY = InScrollSpeed.mY;
    UpdateTickEnabled();
}

void UScrollUVComponent::DrawPanels(IPropertyEditorContext* Context) {
    UBillboardComponent::DrawPanels(Context);

    Context->DrawVector2("ScrollSpeed", mScrollSpeed, 0.01f, -5.0f, 5.0f, [this](FVector2 NewSpeed) {
        SetScrollSpeed(NewSpeed);
    });
}
