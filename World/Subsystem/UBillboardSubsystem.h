#pragma once
#include "World/Subsystem/UWorldSubsystem.h"

#include "Core/Base/FRenderProbe.h"
#include "World/Component/UBillboardComponent.h"

class UBillboardSubsystem : public UWorldSubsystem {
public:
    UBillboardSubsystem() = default;
    ~UBillboardSubsystem() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UBillboardSubsystem, UWorldSubsystem);

    void RegisterComponent(UBillboardComponent* Component);
    void UnregisterComponent(UBillboardComponent* Component);
    void BuildRenderProbes(FSceneRenderData& Scene) const;

    bool ContainsComponent(const UBillboardComponent* Component);
    const TArray<UBillboardComponent*>& GetRegisteredComponents() const;

private:
    void OnDeinitialize() override;

private:
    TArray<UBillboardComponent*> mComponents{};
};
