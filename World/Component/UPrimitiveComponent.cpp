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

void UPrimitiveComponent::UpdateBounds()
{
    const DirectX::XMMATRIX WorldMatrix = GetComponentToWorld().ToSimpleMath();
    mLocalSphere.Transform(mWorldSphere, WorldMatrix);
    mPickingBox.Transform(mWorldOBB, WorldMatrix);
    mLocalAABB.Transform(mWorldAABB, WorldMatrix);

    mWorldBoundsDirty = false;
}

void UPrimitiveComponent::Serialize(FArchive& Archive) {
    USceneComponent::Serialize(Archive);

    Archive.Serialize("bVisible", mBVisible);
}

void UPrimitiveComponent::SetPickingBox(const DirectX::BoundingOrientedBox& Box) {
    mPickingBox = Box;

    BuildBoundsFromOBB();
}

const DirectX::BoundingOrientedBox& UPrimitiveComponent::GetPickingBox() const {
    return mPickingBox;
}

void UPrimitiveComponent::BuildBoundsFromOBB()
{
    DirectX::BoundingSphere::CreateFromBoundingBox(mLocalSphere, mPickingBox);
    DirectX::BoundingBox::CreateFromSphere(mLocalAABB, mLocalSphere);
}

const DirectX::BoundingSphere& UPrimitiveComponent::GetBoundingSphere() const
{
    return mLocalSphere;
}

const DirectX::BoundingBox& UPrimitiveComponent::GetWorldAABB() const
{
    if (mWorldBoundsDirty)
    {
        const_cast<UPrimitiveComponent*>(this)->UpdateBounds();
    }
    return mWorldAABB;
}

const DirectX::BoundingOrientedBox& UPrimitiveComponent::GetWorldOBB() const
{
    if (mWorldBoundsDirty)
    {
        const_cast<UPrimitiveComponent*>(this)->UpdateBounds();
    }
    return mWorldOBB;
}

const DirectX::BoundingSphere& UPrimitiveComponent::GetWorldSphere() const
{
    if (mWorldBoundsDirty)
    {
        const_cast<UPrimitiveComponent*>(this)->UpdateBounds();
    }
    return mWorldSphere;
}

void UPrimitiveComponent::OnTransformUpdate()
{
    mWorldBoundsDirty = true;
}

void UPrimitiveComponent::DrawPanels(IPropertyEditorContext* Context) {
    USceneComponent::DrawPanels(Context);
    Context->DrawBool("Visible", IsVisible(), [this](bool BVisible) {
        SetVisible(BVisible);
    });
}
