#pragma once

#include "UWorldSubsystem.h"

#include "Core/Base/FRenderProbe.h"
#include "World/Component/ULightComponent.h"

class ULightSubsystem : public UWorldSubsystem {
public:
    ULightSubsystem() = default;
    ~ULightSubsystem() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(ULightSubsystem, UWorldSubsystem)

    void RegisterComponent(ULightComponent* Component);
    void UnregisterComponent(ULightComponent* Component);
    void BuildLightProbes(FSceneRenderData& Scene) const;

    bool ContainsComponent(const ULightComponent* Component) const;
    const TArray<ULightComponent*>& GetRegisteredComponents() const;

protected:
    void OnDeinitialize() override;

private:
    TArray<ULightComponent*> mComponents{};
};
