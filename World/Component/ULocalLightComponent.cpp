#include "pch.h"
#include <cfloat>
#include "Core/Property/IPropertyEditorContext.h"

#include "ULocalLightComponent.h"

float ULocalLightComponent::GetAttenuationRadius() const {
    return mAttenuationRadius;
}

void ULocalLightComponent::SetAttenuationRadius(float InAttenuationRadius) {
    mAttenuationRadius = std::max(InAttenuationRadius, 0.0f);
}

void ULocalLightComponent::MakeLightProbe(FLightProbe& OutProbe) const {
    ULightComponent::MakeLightProbe(OutProbe);
    OutProbe.mAttenuationRadius = GetAttenuationRadius();
}

void ULocalLightComponent::Serialize(FArchive& Archive) {
    ULightComponent::Serialize(Archive);
    Archive.Serialize("AttenuationRadius", mAttenuationRadius);
}

void ULocalLightComponent::DrawPanels(IPropertyEditorContext* Context) {
    ULightComponent::DrawPanels(Context);

    if (!Context->BeginCategory("Local Light")) {
        return;
    }

    Context->DrawFloat("Attenuation Radius", GetAttenuationRadius(), 1.0f, 0.0f, FLT_MAX, [this](float InAttenuationRadius) {
        SetAttenuationRadius(InAttenuationRadius);
    });
}
