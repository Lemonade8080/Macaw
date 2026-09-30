#include "pch.h"
#include "AActor.h"
#include "Core/Stat/Stat.h"
#include "World/UWorld.h"
#include "Component/USceneComponent.h"
#include "../Core/Base/TypeRegistry.h"

const std::vector<std::unique_ptr<UActorComponent>>& AActor::GetComponents() const {
    return mComponents;
}

AActor::~AActor() {
    SetWorld(nullptr);

    for (std::unique_ptr<UActorComponent>& Component : mComponents) {
        Component->UnregisterComponent();
        UObjectSystem::Unregister(Component.get(), Component->GetHandle());
    }
}

UActorComponent* AActor::AddComponent(const FTypeInfo& Type) {
    if (Type.mCreator == nullptr) {
        return nullptr;
    }

    std::unique_ptr<UObject> CreatedObject{Type.mCreator()};
    if (CreatedObject == nullptr ||
        !CreatedObject->GetTypeInfo()->IsA(UActorComponent::StaticTypeInfo())) {
        return nullptr;
    }

    std::unique_ptr<UActorComponent> NewComponent{ static_cast<UActorComponent*>(CreatedObject.release())};
    UActorComponent* ComponentPtr{NewComponent.get()};

    ComponentPtr->SetOwner(this);
    UObjectSystem::Register(ComponentPtr);
    mComponents.push_back(std::move(NewComponent));

    if (mWorld != nullptr) {
        ComponentPtr->RegisterComponent(mWorld);

        if (mBHasBegunPlay) {
            ComponentPtr->InitializeComponent();
            ComponentPtr->BeginPlay();
        }
    }

    return ComponentPtr;
}

void AActor::RemoveOwnedComponent(UActorComponent* Component) {
    auto It{std::ranges::find_if(mComponents, [Component](const std::unique_ptr<UActorComponent>& Ptr) {
        return Ptr.get() == Component;
    })};

    if (It == mComponents.end()) {
        return;
    }

    if (mRootComponent == Component) {
        mRootComponent = nullptr;
    }

    UnregisterTickComponent(Component);
    UObjectSystem::Unregister(Component, Component->GetHandle());
    mComponents.erase(It);
}

USceneComponent* AActor::GetRootComponent() {
    return mRootComponent;
}

const USceneComponent* AActor::GetRootComponent() const {
    return mRootComponent;
}

bool AActor::SetRootComponent(USceneComponent* InRootComponent) {
    if (InRootComponent != nullptr) {
        const bool BIsOwnedComponent{std::ranges::any_of(mComponents, [InRootComponent](const std::unique_ptr<UActorComponent>& Component) {
            return Component.get() == InRootComponent;
        })};

        if (!BIsOwnedComponent) {
            return false;
        }
    }

    mRootComponent = InRootComponent;
    return true;
}

void AActor::SetWorld(UWorld* InWorld) {
    if (mWorld == InWorld) {
        return;
    }

    if (mWorld != nullptr) {
        if (mBHasBegunPlay) {
            EndPlay();
            mBHasBegunPlay = false;
        }

        mWorld->UnregisterTickActor(this);
        for (const std::unique_ptr<UActorComponent>& Component : mComponents) {
            Component->UnregisterComponent();
        }

        OnRemovedFromWorld();
        mWorld = nullptr;
    }

    if (InWorld == nullptr) {
        return;
    }

    mWorld = InWorld;
    OnAddedToWorld();

    for (const std::unique_ptr<UActorComponent>& Component : mComponents) {
        Component->RegisterComponent(mWorld);
    }

    InitializeComponents();
    BeginPlay();
    mBHasBegunPlay = true;
    UpdateTickRegistration();
}

UWorld* AActor::GetWorld() const {
    return mWorld;
}

bool AActor::Destroy() {
    return mWorld != nullptr && mWorld->DestroyActor(this);
}

FTransform AActor::GetActorTransform() const {
    if (mRootComponent == nullptr) {
        return {};
    }

    return mRootComponent->GetComponentTransform();
}

bool AActor::SetActorTransform(const FTransform& Transform) {
    if (mRootComponent == nullptr) {
        return false;
    }

    return mRootComponent->SetWorldTransform(Transform);
}

FVector3 AActor::GetActorLocation() const {
    if (mRootComponent == nullptr) {
        return FVector3::Zero;
    }

    return mRootComponent->GetComponentLocation();
}

bool AActor::SetActorLocation(const FVector3& Location) {
    if (mRootComponent == nullptr) {
        return false;
    }

    return mRootComponent->SetWorldLocation(Location);
}

bool AActor::SetActorLocationAndRotation(const FVector3& Location, const FRotator& Rotation) {
    return mRootComponent != nullptr && mRootComponent->SetWorldLocationAndRotation(Location, Rotation);
}

FRotator AActor::GetActorRotation() const {
    if (mRootComponent == nullptr) {
        return FRotator::Zero;
    }

    return mRootComponent->GetComponentRotation();
}

bool AActor::SetActorRotation(const FRotator& Rotation) {
    return mRootComponent != nullptr && mRootComponent->SetWorldRotation(Rotation);
}

FVector3 AActor::GetActorScale3D() const {
    if (mRootComponent == nullptr) {
        return {1.0f, 1.0f, 1.0f};
    }

    return mRootComponent->GetComponentScale();
}

bool AActor::SetActorScale3D(const FVector3& Scale) {
    return mRootComponent != nullptr && mRootComponent->SetWorldScale3D(Scale);
}

FTransform AActor::GetActorRelativeTransform() const {
    return mRootComponent != nullptr ? mRootComponent->GetRelativeTransform() : FTransform{};
}

bool AActor::SetActorRelativeTransform(const FTransform& Transform) {
    if (mRootComponent == nullptr) {
        return false;
    }

    mRootComponent->SetRelativeTransform(Transform);
    return true;
}

FVector3 AActor::GetActorRelativeLocation() const {
    return mRootComponent != nullptr ? mRootComponent->GetRelativeLocation() : FVector3::Zero;
}

bool AActor::SetActorRelativeLocation(const FVector3& Location) {
    if (mRootComponent == nullptr) {
        return false;
    }

    mRootComponent->SetRelativeLocation(Location);
    return true;
}

bool AActor::SetActorRelativeLocationAndRotation(const FVector3& Location, const FRotator& Rotation) {
    if (mRootComponent == nullptr) {
        return false;
    }

    mRootComponent->SetRelativeLocationAndRotation(Location, Rotation);
    return true;
}

FRotator AActor::GetActorRelativeRotation() const {
    return mRootComponent != nullptr ? mRootComponent->GetRelativeRotation() : FRotator::Zero;
}

bool AActor::SetActorRelativeRotation(const FRotator& Rotation) {
    if (mRootComponent == nullptr) {
        return false;
    }

    mRootComponent->SetRelativeRotation(Rotation);
    return true;
}

FVector3 AActor::GetActorRelativeScale3D() const {
    return mRootComponent != nullptr ? mRootComponent->GetRelativeScale3D() : FVector3{1.0f, 1.0f, 1.0f};
}

bool AActor::SetActorRelativeScale3D(const FVector3& Scale) {
    if (mRootComponent == nullptr) {
        return false;
    }

    mRootComponent->SetRelativeScale3D(Scale);
    return true;
}

bool AActor::HasBegunPlay() const {
    return mBHasBegunPlay;
}

bool AActor::IsTickEnabled() const {
    return mBTickEnabled;
}

void AActor::SetTickEnabled(bool TickEnabled) {
    if (mBTickEnabled == TickEnabled) {
        return;
    }

    mBTickEnabled = TickEnabled;
    UpdateTickRegistration();
}

void AActor::UpdateTickRegistration() {
    if (mWorld == nullptr) {
        return;
    }

    if (mBHasBegunPlay && (mBTickEnabled || mTickComponentCount > 0)) {
        mWorld->RegisterTickActor(this);
    } else {
        mWorld->UnregisterTickActor(this);
    }
}

void AActor::UpdateComponentTickRegistration(UActorComponent* Component) {
    if (Component->mOwner != this || !Component->mBTickEnabled || !Component->mBActive || !Component->mBRegistered || !Component->mBHasBegunPlay || Component->mBIsBeingDestroyed) {
        UnregisterTickComponent(Component);
        return;
    }

    if (Component->mTickIndex != std::numeric_limits<std::size_t>::max()) {
        return;
    }

    mTickComponents.push_back(Component);
    Component->mTickIndex = mTickComponents.size() - 1;
    ++mTickComponentCount;
    UpdateTickRegistration();
}

void AActor::UnregisterTickComponent(UActorComponent* Component) {
    const std::size_t Index{Component->mTickIndex};
    if (Index == std::numeric_limits<std::size_t>::max()) {
        return;
    }

    Component->mTickIndex = std::numeric_limits<std::size_t>::max();
    --mTickComponentCount;
    if (mBTickingComponents) {
        mTickComponents[Index] = nullptr;
        mTickComponentsNeedCompaction = true;
    } else {
        if (Index + 1 < mTickComponents.size()) {
            UActorComponent* LastComponent{mTickComponents.back()};
            mTickComponents[Index] = LastComponent;
            LastComponent->mTickIndex = Index;
        }
        mTickComponents.pop_back();
    }
    UpdateTickRegistration();
}

void AActor::FinishComponentTicks(bool WasTicking) {
    mBTickingComponents = WasTicking;
    if (WasTicking || !mTickComponentsNeedCompaction) {
        return;
    }

    std::erase(mTickComponents, nullptr);
    for (std::size_t Index{}; Index < mTickComponents.size(); ++Index) {
        mTickComponents[Index]->mTickIndex = Index;
    }
    mTickComponentsNeedCompaction = false;
}

void AActor::Tick(float DeltaTime) {
    if (!mBHasBegunPlay) {
        return;
    }

    Stat::FWorldTickStats* TickStats{Stat::GetActiveWorldTickStats()};
    const bool WasTicking{mBTickingComponents};
    mBTickingComponents = true;
    const std::size_t ComponentCount{mTickComponents.size()};
    try {
        for (std::size_t Index{}; Index < ComponentCount && Index < mTickComponents.size(); ++Index) {
            UActorComponent* Component{mTickComponents[Index]};
            if (Component == nullptr) {
                continue;
            }
            if (TickStats != nullptr) {
                ++TickStats->mComponentVisitCount;
                ++TickStats->mComponentTickCount;
            }
            Component->Tick(DeltaTime);
        }
    } catch (...) {
        FinishComponentTicks(WasTicking);
        throw;
    }
    FinishComponentTicks(WasTicking);
}

void AActor::Serialize(FArchive& Archive) {
    UObject::Serialize(Archive);

    // components
    std::size_t ArraySize{mComponents.size()};
    Archive.BeginArrayScope("Components", ArraySize);

    for (std::size_t I{0}; I < ArraySize; ++I) {
        Archive.BeginObjectScope(std::to_string(I));
        mComponents[I]->Serialize(Archive);
        Archive.EndObjectScope();
    }
    Archive.EndArrayScope();

    // root component
    FString GuidRootComponent{};
    if (mRootComponent != nullptr) {
        GuidRootComponent = mRootComponent->GetGuid().ToString();
    }

    Archive.Serialize("GuidRootComponent", GuidRootComponent);
    if (Archive.IsLoading()) {
        mRootComponent = nullptr;
        mPendingRootComponentGuid = {};

        if (!GuidRootComponent.empty() && !mPendingRootComponentGuid.Parse(GuidRootComponent)) {
            mPendingRootComponentGuid = {};
        }
    }
}

void AActor::OnAddedToWorld() {
}

void AActor::InitializeComponents() {
    for (const std::unique_ptr<UActorComponent>& Component : mComponents) {
        if (Component->IsRegistered() && !Component->IsInitialized()) {
            Component->InitializeComponent();
        }
    }
}

void AActor::BeginPlay() {
    for (const std::unique_ptr<UActorComponent>& Component : mComponents) {
        if (Component->IsRegistered() && !Component->HasBegunPlay()) {
            Component->BeginPlay();
        }
    }
}

void AActor::EndPlay() {
    for (auto It{mComponents.rbegin()}; It != mComponents.rend(); ++It) {
        UActorComponent* Component{It->get()};
        if (Component->HasBegunPlay()) {
            Component->EndPlay();
        }
    }
}

void AActor::OnRemovedFromWorld() {
}

bool AActor::PreLoadComponents(FArchive& Archive) {
    std::size_t ArraySize{0};
    Archive.BeginArrayScope("Components", ArraySize);

    mTickComponents.clear();
    mTickComponentCount = 0;
    mTickComponentsNeedCompaction = false;
    UpdateTickRegistration();
    mComponents.clear();
    mComponents.reserve(ArraySize);

    for (std::size_t I{0}; I < ArraySize; ++I) {
        Archive.BeginObjectScope(std::to_string(I));

        FString TypeName{};
        Archive.Serialize("TypeName", TypeName);

        const FTypeInfo* Type{TypeRegistry::Find(TypeName)};
        if (Type == nullptr || Type->mCreator == nullptr) {
            Archive.EndObjectScope();
            Archive.EndArrayScope();
            return false;
        }

        std::unique_ptr<UObject> CreatedObject{Type->mCreator()};
        if (CreatedObject == nullptr ||
            !CreatedObject->GetTypeInfo()->IsA(UActorComponent::StaticTypeInfo())) {
            Archive.EndObjectScope();
            Archive.EndArrayScope();
            return false;
        }

        std::unique_ptr<UActorComponent> Component{ static_cast<UActorComponent*>(CreatedObject.release())};
        Component->SetOwner(this);

        FGuid ComponentGuid{};
        Archive.Serialize("Guid", ComponentGuid);
        UObjectSystem::RegisterWithGuid(Component.get(), ComponentGuid);
        mComponents.push_back(std::move(Component));

        Archive.EndObjectScope();
    }

    Archive.EndArrayScope();
    return true;
}

bool AActor::ResolveLoadedReferences() {
    if (mPendingRootComponentGuid.IsValid()) {
        UObject* ResolvedObject{UObjectSystem::Resolve(UObjectSystem::FindHandleByGuid(mPendingRootComponentGuid))};

        if (ResolvedObject == nullptr ||
            !ResolvedObject->GetTypeInfo()->IsA(USceneComponent::StaticTypeInfo())) {
            return false;
        }

        mRootComponent = static_cast<USceneComponent*>(ResolvedObject);
    }

    for (const std::unique_ptr<UActorComponent>& Component : mComponents) {
        if (!Component->ResolveLoadedReferences()) {
            return false;
        }
    }

    return true;
}
