#pragma once

#include "UWorldSubsystem.h"

#include "Core/Base/FRenderProbe.h"
#include "World/Component/UStaticMeshComponent.h"

#include <unordered_map>
#include <unordered_set>

/// <summary>Builds render probes from registered StaticMeshComponents.</summary>
class URenderSubsystem : public UWorldSubsystem {
public:
    URenderSubsystem();
    ~URenderSubsystem() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(URenderSubsystem, UWorldSubsystem)

    void RegisterComponent(UStaticMeshComponent* Component);
    void UnregisterComponent(UStaticMeshComponent* Component);
    void UpdateComponentRenderState(UStaticMeshComponent* Component);

    void BuildRenderProbes(FSceneRenderData& Scene);

    bool ContainsComponent(const UStaticMeshComponent* Component) const;
    const TArray<UStaticMeshComponent*>& GetRegisteredComponents() const;

    Uint64 GetSceneId() const;

private:
    static Uint64 GetComponentKey(FObjectHandle Handle);
    void MarkComponentDirty(FObjectHandle Handle);

    void OnDeinitialize() override;

private:
    TArray<UStaticMeshComponent*> mComponents{};
    std::unordered_map<Uint64, std::size_t> mComponentIndices{};

    TArray<FObjectHandle> mDirtyComponents{};
    std::unordered_set<Uint64> mDirtyComponentKeys{};

    Uint64 mSceneId{};
    Uint64 mRevision{};
};
