#include "pch.h"
#include <cfloat>
#include "Core/Property/IPropertyEditorContext.h"

#include "ULightComponentBase.h"

const FVector3& ULightComponentBase::GetLightColor() const {
    return mLightColor;
}

void ULightComponentBase::SetLightColor(const FVector3& InLightColor) {
    mLightColor = InLightColor;
}

float ULightComponentBase::GetIntensity() const {
    return mIntensity;
}

void ULightComponentBase::SetIntensity(float InIntensity) {
    mIntensity = std::max(InIntensity, 0.0f);
}

bool ULightComponentBase::IsVisible() const {
    return mBVisible;
}

void ULightComponentBase::SetVisible(bool BInVisible) {
    mBVisible = BInVisible;
}

void ULightComponentBase::Serialize(FArchive& Archive) {
    USceneComponent::Serialize(Archive);

    Archive.Serialize("LightColor", mLightColor);
    Archive.Serialize("Intensity", mIntensity);
    Archive.Serialize("bVisible", mBVisible);
}

void ULightComponentBase::DrawPanels(IPropertyEditorContext* Context) {
    USceneComponent::DrawPanels(Context);

    if (!Context->BeginCategory("Light")) {
        return;
    }

    Context->DrawColor("Color", FVector4{mLightColor, 1.0f}, [this](const FVector4& Color) {
        SetLightColor(FVector3{Color.mX, Color.mY, Color.mZ});
    });
    Context->DrawFloat("Intensity", GetIntensity(), 0.1f, 0.0f, FLT_MAX, [this](float InIntensity) {
        SetIntensity(InIntensity);
    });
    Context->DrawBool("Visible", IsVisible(), [this](bool BInVisible) {
        SetVisible(BInVisible);
    });
}
