#pragma once
#include "UBillboardComponent.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "Core/Archive/FArchive.h"

class UScrollUVComponent final : public UBillboardComponent {
public:
    UScrollUVComponent() = default;
    ~UScrollUVComponent() = default;

    JG_DECLARE_DERIVED_TYPEINFO(UScrollUVComponent, UBillboardComponent);

    UScrollUVComponent(const UScrollUVComponent&) = delete;
    UScrollUVComponent& operator=(const UScrollUVComponent&) = delete;

    UScrollUVComponent(const UScrollUVComponent&&) = delete;
    UScrollUVComponent& operator=(const UScrollUVComponent&&) = delete;

    bool IsPlaying() const;

    bool IsLooping() const;

    void PlayScrollUV();

    void PauseScrollUV();

    void SetScrollSpeed(FVector2 InScrollSpeed);

    void Tick(float DeltaTime) override;
    void DrawPanels(IPropertyEditorContext* Context) override;
    void UpdateUVFromCurrentFrame();

protected:
    void Serialize(FArchive& Archive) override;

private:
    FVector2 mScrollSpeed{0.1f, 0.1f};
    FVector2 mCurrentOffset{0.0f, 0.0f};
    bool mBPlaying{true};
    bool mBLooping{true};
};
