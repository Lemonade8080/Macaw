#pragma once

#include "UWorldSubsystem.h"

#include "Core/Base/FRenderProbe.h"
#include "World/Component/UStaticMeshComponent.h"

class URenderSubsystem : public UWorldSubsystem {
public:
    URenderSubsystem() = default;
    ~URenderSubsystem() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(URenderSubsystem, UWorldSubsystem)

    void RegisterComponent(UStaticMeshComponent* Component);
    void UnregisterComponent(UStaticMeshComponent* Component);
    void BuildRenderProbes(FRenderProbe& Probe) const;

    bool ContainsComponent(const UStaticMeshComponent* Component) const;
    const TArray<UStaticMeshComponent*>& GetRegisteredComponents() const;

private:
    void OnDeinitialize() override;

private:
    TArray<UStaticMeshComponent*> mComponents{};
};
