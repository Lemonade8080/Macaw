#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"

#include "USpotLightComponent.h"

#include <numbers>

namespace {
    constexpr float MinimumConeAngle{0.0f};
    constexpr float MaximumConeAngle{89.9f};

    float ToRadians(float Degrees) {
        return Degrees * (std::numbers::pi_v<float> / 180.0f);
    }
}

ELightType USpotLightComponent::GetLightType() const {
    return ELightType::Spot;
}

float USpotLightComponent::GetInnerConeAngle() const {
    return mInnerConeAngle;
}

float USpotLightComponent::GetOuterConeAngle() const {
    return mOuterConeAngle;
}

void USpotLightComponent::SetInnerConeAngle(float InInnerConeAngle) {
    mInnerConeAngle = std::clamp(InInnerConeAngle, MinimumConeAngle, mOuterConeAngle);
}

void USpotLightComponent::SetOuterConeAngle(float InOuterConeAngle) {
    mOuterConeAngle = std::clamp(InOuterConeAngle, mInnerConeAngle, MaximumConeAngle);
}

void USpotLightComponent::MakeLightProbe(FLightProbe& OutProbe) const {
    UPointLightComponent::MakeLightProbe(OutProbe);
    OutProbe.mInnerConeCos = std::cos(ToRadians(mInnerConeAngle));
    OutProbe.mOuterConeCos = std::cos(ToRadians(mOuterConeAngle));
}

void USpotLightComponent::Serialize(FArchive& Archive) {
    UPointLightComponent::Serialize(Archive);
    Archive.Serialize("InnerConeAngle", mInnerConeAngle);
    Archive.Serialize("OuterConeAngle", mOuterConeAngle);

    if (Archive.IsLoading()) {
        SetInnerConeAngle(mInnerConeAngle);
        SetOuterConeAngle(mOuterConeAngle);
    }
}

void USpotLightComponent::DrawPanels(IPropertyEditorContext* Context) {
    UPointLightComponent::DrawPanels(Context);

    if (!Context->BeginCategory("Spot Light")) {
        return;
    }

    Context->DrawFloat("Inner Cone Angle", GetInnerConeAngle(), 0.1f, MinimumConeAngle, GetOuterConeAngle(), [this](float InInnerConeAngle) {
        SetInnerConeAngle(InInnerConeAngle);
    });
    Context->DrawFloat("Outer Cone Angle", GetOuterConeAngle(), 0.1f, GetInnerConeAngle(), MaximumConeAngle, [this](float InOuterConeAngle) {
        SetOuterConeAngle(InOuterConeAngle);
    });
}
