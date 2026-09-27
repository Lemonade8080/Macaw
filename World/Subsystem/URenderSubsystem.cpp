#include "pch.h"

#include "URenderSubsystem.h"

#include "World/AActor.h"
#include "World/UWorld.h"
#include "World/Component/UStaticMeshComponent.h"
#include "World/FWorldEditorContext.h"

#include <algorithm>
#include <tuple>

void URenderSubsystem::RegisterComponent(UStaticMeshComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) {
        return;
    }

    const auto Position{std::upper_bound(mComponents.begin(), mComponents.end(), Component, IsComponentLess)};
    mComponents.insert(Position, Component);
}

void URenderSubsystem::UnregisterComponent(UStaticMeshComponent* Component) {
    std::erase(mComponents, Component);
}

void URenderSubsystem::UpdateComponentRenderState(UStaticMeshComponent* Component) {
    const auto Position{std::ranges::find(mComponents, Component)};
    if (Position == mComponents.end()) {
        return;
    }

    const bool BeforePrevious{Position != mComponents.begin() && IsComponentLess(Component, *(Position - 1))};
    const bool AfterNext{Position + 1 != mComponents.end() && IsComponentLess(*(Position + 1), Component)};
    if (!BeforePrevious && !AfterNext) {
        return;
    }

    mComponents.erase(Position);
    const auto NewPosition{std::upper_bound(mComponents.begin(), mComponents.end(), Component, IsComponentLess)};
    mComponents.insert(NewPosition, Component);
}

void URenderSubsystem::BuildRenderProbes(FRenderProbe& Probe) const {
    Probe.mActorProbes.clear();
    Probe.mGizmoProbes.clear();

    const FWorldEditorContext* EditorContext{GetWorld()->GetEditorContext()};
    const AActor* SelectedActor{EditorContext != nullptr ? EditorContext->GetSelectedActor() : nullptr};
    for (const UStaticMeshComponent* Component : mComponents) {
        if (!Component->IsActive() || !Component->IsVisible()) {
            continue;
        }

        FActorProbe ActorProbe{};
        Component->MakeRender(ActorProbe);

        if (SelectedActor != nullptr && Component->GetOwner() == SelectedActor) {
            ActorProbe.mFlags |= static_cast<Uint32>(ERenderObjectFlags::Selected);
        }

        Probe.mActorProbes.push_back(ActorProbe);
    }
}

bool URenderSubsystem::ContainsComponent(const UStaticMeshComponent* Component) const {
    return std::ranges::find(mComponents, Component) != mComponents.end();
}

const TArray<UStaticMeshComponent*>& URenderSubsystem::GetRegisteredComponents() const {
    return mComponents;
}

bool URenderSubsystem::IsComponentLess(const UStaticMeshComponent* Left, const UStaticMeshComponent* Right) {
    const FAssetHandle LeftPipeline{Left->GetPipelineHandle()};
    const FAssetHandle RightPipeline{Right->GetPipelineHandle()};
    const FAssetHandle LeftMaterial{Left->GetMaterialHandle()};
    const FAssetHandle RightMaterial{Right->GetMaterialHandle()};
    const FAssetHandle LeftMesh{Left->GetMeshHandle()};
    const FAssetHandle RightMesh{Right->GetMeshHandle()};
    return std::tie(LeftPipeline.mId, LeftPipeline.mGeneration, LeftMaterial.mId, LeftMaterial.mGeneration, LeftMesh.mId, LeftMesh.mGeneration) < std::tie(RightPipeline.mId, RightPipeline.mGeneration, RightMaterial.mId, RightMaterial.mGeneration, RightMesh.mId, RightMesh.mGeneration);
}

void URenderSubsystem::OnDeinitialize() {
    mComponents.clear();
}
