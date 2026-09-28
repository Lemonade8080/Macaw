#pragma once

#include "UWorldSubsystem.h"

#include "Core/Base/FRenderProbe.h"
#include "World/Component/UStaticMeshComponent.h"

/// <summary>Builds render probes from registered StaticMeshComponents.</summary>
class URenderSubsystem : public UWorldSubsystem {
public:
    URenderSubsystem() = default;
    ~URenderSubsystem() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(URenderSubsystem, UWorldSubsystem)

    void RegisterComponent(UStaticMeshComponent* Component);
    void UnregisterComponent(UStaticMeshComponent* Component);
    void UpdateComponentRenderState(UStaticMeshComponent* Component);
    void BuildRenderProbes(FSceneRenderData& Scene) const;

    bool ContainsComponent(const UStaticMeshComponent* Component) const;
    const TArray<UStaticMeshComponent*>& GetRegisteredComponents() const;

private:
    static bool IsComponentLess(const UStaticMeshComponent* Left, const UStaticMeshComponent* Right);

    void OnDeinitialize() override;

private:
    TArray<UStaticMeshComponent*> mComponents{};
};
