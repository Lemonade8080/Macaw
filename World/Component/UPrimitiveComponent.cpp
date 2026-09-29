#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UPrimitiveComponent.h"

#include "World/AActor.h"
#include "World/Subsystem/UPickingSubsystem.h"
#include "World/UWorld.h"

void UPrimitiveComponent::MakeRender(FActorProbe& OutProbe) const {
}

bool UPrimitiveComponent::IsVisible() const {
    return mBVisible;
}

void UPrimitiveComponent::SetVisible(bool BInVisible) {
    mBVisible = BInVisible;
}

void UPrimitiveComponent::OnRegister() {
    USceneComponent::OnRegister();

    AActor* Owner{GetOwner()};
    if (Owner != nullptr && Owner->GetWorld() != nullptr) {
        Owner->GetWorld()->GetPickingSubsystem().RegisterComponent(this);
    }
}

void UPrimitiveComponent::OnUnregister() {
    AActor* Owner{GetOwner()};
    if (Owner != nullptr && Owner->GetWorld() != nullptr) {
        Owner->GetWorld()->GetPickingSubsystem().UnregisterComponent(this);
    }

    USceneComponent::OnUnregister();
}

void UPrimitiveComponent::Serialize(FArchive& Archive) {
    USceneComponent::Serialize(Archive);

    Archive.Serialize("bVisible", mBVisible);
}

void UPrimitiveComponent::SetPickingBox(const DirectX::BoundingOrientedBox& Box) {
    mPickingBox = Box;

    BuildBoundingSphereFromOBB();
}

const DirectX::BoundingOrientedBox& UPrimitiveComponent::GetPickingBox() const {
    return mPickingBox;
}

void UPrimitiveComponent::BuildBoundingSphereFromOBB()
{
    DirectX::BoundingSphere::CreateFromBoundingBox(mBoundingSphere, mPickingBox);
}

const DirectX::BoundingSphere& UPrimitiveComponent::GetBoundingSphere() const
{
    return mBoundingSphere;
}

void UPrimitiveComponent::DrawPanels(IPropertyEditorContext* Context) {
    USceneComponent::DrawPanels(Context);
    Context->DrawBool("Visible", IsVisible(), [this](bool BVisible) {
        SetVisible(BVisible);
    });
}
