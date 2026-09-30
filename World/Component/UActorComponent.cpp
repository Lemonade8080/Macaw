#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UActorComponent.h"
#include "../AActor.h"
#include "Core/Base/ErrorHandler.h"

AActor* UActorComponent::GetOwner() const {
    return mOwner;
}

void UActorComponent::SetOwner(AActor* InOwner) {
    mOwner = InOwner;
}

void UActorComponent::OnRegister() {
}

void UActorComponent::InitializeComponent() {
    mBInitialized = true;
}

void UActorComponent::BeginPlay() {
    mBHasBegunPlay = true;
}

void UActorComponent::EndPlay() {
    mBHasBegunPlay = false;
}

void UActorComponent::Tick(float /*DeltaTime*/  ) {
}

void UActorComponent::OnUnregister() {
}

void UActorComponent::OnRenderStateChanged() {
}

bool UActorComponent::IsActive() const {
    return mBActive;
}

void UActorComponent::SetActive(bool BInActive) {
    if (mBActive == BInActive) {
        return;
    }

    mBActive = BInActive;
    OnRenderStateChanged();
}

bool UActorComponent::IsRegistered() const {
    return mBRegistered;
}

bool UActorComponent::IsInitialized() const {
    return mBInitialized;
}

bool UActorComponent::HasBegunPlay() const {
    return mBHasBegunPlay;
}

UWorld* UActorComponent::GetBelongingWorld() const {
    return mParentWorld;
}

void UActorComponent::RegisterComponent(UWorld* World) {
    ErrorHandler::Report(mOwner == nullptr and mParentWorld == nullptr, "[ UActorComponent ]", "Owner and ParentWorld must not be null.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(World != mOwner->GetWorld(), "[ UActorComponent ]", "World must match Owner's world.", ErrorHandler::EErrorLevel::Critical);

    if (mBRegistered)
        return;

    mParentWorld = World;
    mBRegistered = true;

    this->OnRegister();
}

void UActorComponent::UnregisterComponent() {
    if (not mBRegistered) {
        return;
    }

    ErrorHandler::Report(mOwner == nullptr or mParentWorld == nullptr, "[ UActorComponent ]", "Owner and ParentWorld must not be null.", ErrorHandler::EErrorLevel::Critical);

    if (mBHasBegunPlay) {
        EndPlay();
    }

    this->OnUnregister();

    mBRegistered = false;
    mParentWorld = nullptr;
}

void UActorComponent::DestroyComponent(bool /*bPromoteChildren*/  ) {
    if (mBIsBeingDestroyed) {
        return;
    }

    mBIsBeingDestroyed = true;

    UnregisterComponent();

    if (mOwner != nullptr) {
        mOwner->RemoveOwnedComponent(this);
    }
}

bool UActorComponent::ResolveLoadedReferences() {
    return true;
}

void UActorComponent::Serialize(FArchive& Archive) {
    UObject::Serialize(Archive);

    Archive.Serialize("bActive", mBActive);

    if (Archive.IsLoading()) {
        OnRenderStateChanged();
    }
}

void UActorComponent::DrawPanels(IPropertyEditorContext* Context) {
    Context->DrawBool("Active", IsActive(), [this](bool BActive) {
        SetActive(BActive);
    });
}
