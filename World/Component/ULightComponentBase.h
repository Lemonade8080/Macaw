#pragma once

#include "USceneComponent.h"

class ULightComponentBase : public USceneComponent {
public:
    ULightComponentBase() = default;
    ~ULightComponentBase() override = default;

    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(ULightComponentBase, USceneComponent) const FVector3& GetLightColor() const;
    void SetLightColor(const FVector3& InLightColor);

    float GetIntensity() const;
    void SetIntensity(float InIntensity);

    bool IsVisible() const;
    void SetVisible(bool BInVisible);

    void DrawPanels(IPropertyEditorContext* Context) override;

protected:
    void Serialize(FArchive& Archive) override;

private:
    FVector3 mLightColor{1.0f, 1.0f, 1.0f};
    float mIntensity{1.0f};
    bool mBVisible{true};
};
