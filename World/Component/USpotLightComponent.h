#pragma once

#include "UPointLightComponent.h"

class USpotLightComponent : public UPointLightComponent {
public:
    USpotLightComponent() = default;
    ~USpotLightComponent() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(USpotLightComponent, UPointLightComponent)

    ELightType GetLightType() const override;

    float GetInnerConeAngle() const;
    float GetOuterConeAngle() const;
    void SetInnerConeAngle(float InInnerConeAngle);
    void SetOuterConeAngle(float InOuterConeAngle);

    void MakeLightProbe(FLightProbe& OutProbe) const override;
    void DrawPanels(IPropertyEditorContext* Context) override;

protected:
    void Serialize(FArchive& Archive) override;

private:
    float mInnerConeAngle{30.0f};
    float mOuterConeAngle{45.0f};
};
