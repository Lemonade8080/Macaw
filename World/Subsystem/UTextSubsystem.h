#pragma once

#include "UWorldSubsystem.h"

#include "Core/Base/FRenderProbe.h"
#include "World/Component/UBillboardTextComponent.h"

/// <summary>Builds render probes from registered BillboardTextComponents.</summary>
class UTextSubsystem : public UWorldSubsystem {
public:
    UTextSubsystem() = default;
    ~UTextSubsystem() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(UTextSubsystem, UWorldSubsystem)

    void RegisterComponent(UBillboardTextComponent* Component);
    void UnregisterComponent(UBillboardTextComponent* Component);
    void BuildTextProbes(FSceneRenderData& Scene) const;

    bool ContainsComponent(const UBillboardTextComponent* Component) const;
    const TArray<UBillboardTextComponent*>& GetRegisteredComponents() const;

protected:
    void OnDeinitialize() override;

private:
    TArray<UBillboardTextComponent*> mComponents{};
};
