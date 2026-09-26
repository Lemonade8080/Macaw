#include "pch.h"

#include "URenderSubsystem.h"

#include "World/AActor.h"
#include "World/UWorld.h"
#include "World/Component/UStaticMeshComponent.h"
#include "World/FWorldEditorContext.h"

#include "Asset/Pipeline/UPipeline.h"

void URenderSubsystem::RegisterComponent(UStaticMeshComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) {
        return;
    }

    mComponents.push_back(Component);
}

void URenderSubsystem::UnregisterComponent(UStaticMeshComponent* Component) {
    std::erase(mComponents, Component);
}

void URenderSubsystem::BuildRenderProbes(FRenderProbe& Probe) const {
    Probe.mActorProbes.clear();
    Probe.mGizmoProbes.clear();

    const FWorldEditorContext* EditorContext{GetWorld()->GetEditorContext()};
    const AActor* SelectedActor{EditorContext != nullptr ? EditorContext->GetSelectedActor() : nullptr};
    for (const UStaticMeshComponent* Component : mComponents) {
        FActorProbe ActorProbe{};
        Component->MakeRender(ActorProbe);

        if (not Component->IsActive() or not Component->IsVisible())
            continue;

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

void URenderSubsystem::OnDeinitialize() {
    mComponents.clear();
}
