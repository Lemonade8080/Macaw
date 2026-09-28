#pragma once

#include "ULightComponent.h"

class ULocalLightComponent : public ULightComponent {
public:
    ULocalLightComponent() = default;
    ~ULocalLightComponent() override = default;

    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(ULocalLightComponent, ULightComponent)

    float GetAttenuationRadius() const;
    void SetAttenuationRadius(float InAttenuationRadius);

    void MakeLightProbe(FLightProbe& OutProbe) const override;
    void DrawPanels(IPropertyEditorContext* Context) override;

protected:
    void Serialize(FArchive& Archive) override;

private:
    float mAttenuationRadius{1000.0f};
};
