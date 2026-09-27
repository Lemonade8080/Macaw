#include "pch.h"
#include "UBillboardSubsystem.h"

#include "World/AActor.h"
#include "World/UWorld.h"
#include "World/Component/UBillboardComponent.h"
#include "World/FWorldEditorContext.h"

#include "Asset/Pipeline/UPipeline.h"

void UBillboardSubsystem::RegisterComponent(UBillboardComponent* Component) {
    if (Component == nullptr || ContainsComponent(Component)) {
        return;
    }

    mComponents.push_back(Component);
}

void UBillboardSubsystem::UnregisterComponent(UBillboardComponent* Component) {
    std::erase(mComponents, Component);
}

void UBillboardSubsystem::BuildRenderProbes(FSceneRenderData& Scene) const {
    Scene.mBillboardProbes.clear();

    for (const UBillboardComponent* Component : mComponents) {
        FBillboardProbe BillboardProbe{};
        if (!Component->MakeBillboardRender(BillboardProbe))
            continue;

        if (not Component->IsActive() or not Component->IsVisible())
            continue;

        Scene.mBillboardProbes.push_back(BillboardProbe);
    }
}

bool UBillboardSubsystem::ContainsComponent(const UBillboardComponent* Component) {
    return std::ranges::find(mComponents, Component) != mComponents.end();
}

const TArray<UBillboardComponent*>& UBillboardSubsystem::GetRegisteredComponents() const {
    return mComponents;
}

void UBillboardSubsystem::OnDeinitialize() {
    mComponents.clear();
}
