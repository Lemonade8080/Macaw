#pragma once

#include "UBillboardTextComponent.h"

#include "Core/Base/FGuid.h"
#include "Core/Base/TObjectRef.h"
#include "World/AActor.h"

class UNameTagComponent final : public UBillboardTextComponent {
public:
    UNameTagComponent() = default;
    ~UNameTagComponent() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UNameTagComponent, UBillboardTextComponent);

    void SetTargetActor(AActor* InTargetActor);

    AActor* GetTargetActor() const;

    void SetTargetLocalOffset(const FVector3& InOffset);
    const FVector3& GetTargetLocalOffset() const;

    FGuid GetObjectGuid() const;

    const FVector3& GetObjectOffset() const;
    bool MakeTextRender(FTextProbe& OutProbe) const override;
    bool ResolveLoadedReferences() override;
    void RefreshGuidText();
    void OnRegister() override;
    void DrawPanels(IPropertyEditorContext* Context) override;

private:
    void Serialize(FArchive& Archive) override;

private:
    TObjectRef<AActor> mTargetActor{};

    FGuid mExplicitTargetGuid{};

    FVector3 mTargetLocalOffset{};
};
