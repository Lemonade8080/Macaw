#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"

#include "USceneComponent.h"
#include "World/AActor.h"

namespace {
    bool ApplyWorldMatrix(USceneComponent& Component, const FMatrix& DesiredWorld) {
        FVector3 Scale{};
        FQuat Rotation{};
        FVector3 Translation{};

        if (!DesiredWorld.Decompose(Scale, Rotation, Translation)) {
            return false;
        }

        return Component.SetWorldTransform(FTransform{Translation, Rotation, Scale});
    }
}

const FTransform& USceneComponent::GetRelativeTransform() const {
    return mTransform;
}

void USceneComponent::SetRelativeTransform(const FTransform& Transform) {
    const Uint64 PreviousRevision{mTransform.GetRevision()};
    mTransform = Transform;
    if (mTransform.GetRevision() != PreviousRevision) {
        MarkTransformDirty();
    }
}

void USceneComponent::SetRelativeLocation(const FVector3& Location) {
    const Uint64 PreviousRevision{mTransform.GetRevision()};
    mTransform.SetPosition(Location);
    if (mTransform.GetRevision() != PreviousRevision) {
        MarkTransformDirty();
    }
}

void USceneComponent::SetRelativeLocationAndRotation(const FVector3& Location, const FRotator& Rotation) {
    const Uint64 PreviousRevision{mTransform.GetRevision()};
    mTransform.SetPosition(Location);
    mTransform.SetRotation(Rotation);
    if (mTransform.GetRevision() != PreviousRevision) {
        MarkTransformDirty();
    }
}

FVector3 USceneComponent::GetRelativeLocation() const {
    return mTransform.GetPosition();
}

void USceneComponent::SetRelativeRotation(const FRotator& Rotation) {
    const Uint64 PreviousRevision{mTransform.GetRevision()};
    mTransform.SetRotation(Rotation);
    if (mTransform.GetRevision() != PreviousRevision) {
        MarkTransformDirty();
    }
}

FRotator USceneComponent::GetRelativeRotation() const {
    return mTransform.GetRotation();
}

void USceneComponent::SetRelativeScale3D(const FVector3& Scale) {
    const Uint64 PreviousRevision{mTransform.GetRevision()};
    mTransform.SetScale(Scale);
    if (mTransform.GetRevision() != PreviousRevision) {
        MarkTransformDirty();
    }
}

FVector3 USceneComponent::GetRelativeScale3D() const {
    return mTransform.GetScale();
}

void USceneComponent::Serialize(FArchive& Archive) {
    UActorComponent::Serialize(Archive);

    Archive.SerializeStruct("Transform", mTransform);

    FGuid ParentGuid{};

    if (Archive.IsSaving() && GetParent() != nullptr) {
        ParentGuid = GetParent()->GetGuid();
    }

    Archive.Serialize("Parent", ParentGuid);

    if (Archive.IsLoading()) {
        mPendingParentGuid = ParentGuid;
        MarkTransformDirty();
    }
}

void USceneComponent::OnUnregister() {
    MarkTransformDirty();
    UActorComponent::OnUnregister();
}

void USceneComponent::DestroyComponent(bool BPromoteChildren) {
    AActor* Actor{GetOwner()};
    if (Actor != nullptr && Actor->GetRootComponent() == this && Actor->Destroy()) {
        return;
    }

    USceneComponent* ParentComponent{GetParent()};
    std::vector<USceneComponent*> ChildrenToDetach{};
    ChildrenToDetach.reserve(mChildren.size());

    for (const TObjectRef<USceneComponent>& ChildRef : mChildren) {
        if (USceneComponent * Child{ChildRef.Get()}) {
            ChildrenToDetach.push_back(Child);
        }
    }

    for (USceneComponent* Child : ChildrenToDetach) {
        Child->AttachToComponent(BPromoteChildren ? ParentComponent : nullptr, EAttachmentTransformRule::KeepWorldTransform);
    }

    DetachFromComponent(EAttachmentTransformRule::KeepWorldTransform);
    UActorComponent::DestroyComponent(BPromoteChildren);
}

void USceneComponent::RemoveChild(USceneComponent* InChild) {
    if (InChild == nullptr) {
        return;
    }

    std::erase_if(mChildren, [InChild](const TObjectRef<USceneComponent>& ChildRef) {
        return ChildRef.Get() == InChild;
    });

    if (InChild->mParent.Get() == this) {
        InChild->mParent.Reset();
        InChild->MarkTransformDirty();
    }
}

bool USceneComponent::AttachToComponent(USceneComponent* ParentComponent, EAttachmentTransformRule Rule) {
    if (ParentComponent == this || mParent.Get() == ParentComponent) {
        return false;
    }

    for (USceneComponent* Ancestor{ParentComponent}; Ancestor != nullptr; Ancestor = Ancestor->GetParent()) {
        if (Ancestor == this) {
            return false;
        }
    }

    const FTransform PreviousWorldTransform{GetComponentTransform()};

    if (USceneComponent * PreviousParent{mParent.Get()}) {
        PreviousParent->RemoveChild(this);
    }

    mParent.Set(ParentComponent);

    if (ParentComponent != nullptr) {
        ParentComponent->mChildren.emplace_back(this);
    }

    MarkTransformDirty();

    if (Rule == EAttachmentTransformRule::KeepWorldTransform) {
        return SetWorldTransform(PreviousWorldTransform);
    }

    return true;
}

bool USceneComponent::DetachFromComponent(EAttachmentTransformRule Rule) {
    return AttachToComponent(nullptr, Rule);
}

bool USceneComponent::SetWorldTransform(const FTransform& WorldTransform) {
    FTransform DesiredWorldTransform{WorldTransform};
    DesiredWorldTransform.SetAbsoluteLocation(mTransform.IsAbsoluteLocation());
    DesiredWorldTransform.SetAbsoluteRotation(mTransform.IsAbsoluteRotation());
    DesiredWorldTransform.SetAbsoluteScale(mTransform.IsAbsoluteScale());

    if (USceneComponent * ParentComponent{mParent.Get()}) {
        FTransform RelativeTransform{};

        if (!DesiredWorldTransform.MakeRelativeTo(ParentComponent->GetComponentTransform(), RelativeTransform)) {
            return false;
        }

        SetRelativeTransform(RelativeTransform);

        return true;
    }

    SetRelativeTransform(DesiredWorldTransform);

    return true;
}

bool USceneComponent::SetWorldTransform(const FMatrix& WorldTransform) {
    return ApplyWorldMatrix(*this, WorldTransform);
}

bool USceneComponent::SetWorldLocation(const FVector3& Location) {
    FTransform DesiredWorldTransform{GetComponentTransform()};
    DesiredWorldTransform.SetPosition(Location);
    return SetWorldTransform(DesiredWorldTransform);
}

bool USceneComponent::SetWorldLocationAndRotation(const FVector3& Location, const FRotator& Rotation) {
    FTransform DesiredWorldTransform{GetComponentTransform()};
    DesiredWorldTransform.SetPosition(Location);
    DesiredWorldTransform.SetRotation(Rotation);
    return SetWorldTransform(DesiredWorldTransform);
}

bool USceneComponent::SetWorldRotation(const FRotator& Rotation) {
    FTransform DesiredWorldTransform{GetComponentTransform()};
    DesiredWorldTransform.SetRotation(Rotation);
    return SetWorldTransform(DesiredWorldTransform);
}

bool USceneComponent::SetWorldScale3D(const FVector3& Scale) {
    FTransform DesiredWorldTransform{GetComponentTransform()};
    DesiredWorldTransform.SetScale(Scale);
    return SetWorldTransform(DesiredWorldTransform);
}

USceneComponent* USceneComponent::GetParent() const {
    return mParent.Get();
}

const std::vector<TObjectRef<USceneComponent>>& USceneComponent::GetChildren() const {
    return mChildren;
}

const FTransform& USceneComponent::GetComponentTransform() const {
    const USceneComponent* ParentComponent{mParent.Get()};
    const FTransform* ParentTransform{ParentComponent != nullptr ? &ParentComponent->GetComponentTransform() : nullptr};
    const FObjectHandle ParentHandle{ParentComponent != nullptr ? ParentComponent->GetHandle() : FObjectHandle{}};
    const Uint64 ParentRevision{ParentComponent != nullptr ? ParentComponent->mTransformRevision : 0};
    if (!mWorldTransformDirty && mCachedParentHandle == ParentHandle && mCachedParentRevision == ParentRevision) {
        return mWorldTransform;
    }

    const bool NotifyChange{!mWorldTransformDirty};
    mWorldTransform = ParentTransform != nullptr ? mTransform.Compose(*ParentTransform) : mTransform;
    mCachedParentHandle = ParentHandle;
    mCachedParentRevision = ParentRevision;
    mWorldTransformDirty = false;
    ++mTransformRevision;
    if (NotifyChange) {
        const_cast<USceneComponent*>(this)->NotifyTransformUpdate();
        return GetComponentTransform();
    }

    return mWorldTransform;
}

Uint64 USceneComponent::GetTransformRevision() const {
    GetComponentTransform();
    return mTransformRevision;
}

FMatrix USceneComponent::GetComponentToWorld() const {
    return GetComponentTransform().ToMatrixWithScale();
}

FVector3 USceneComponent::GetComponentLocation() const {
    return GetComponentTransform().GetPosition();
}

FRotator USceneComponent::GetComponentRotation() const {
    return GetComponentTransform().GetRotation();
}

FVector3 USceneComponent::GetComponentScale() const {
    return GetComponentTransform().GetScale();
}

bool USceneComponent::ResolveLoadedReferences() {
    if (!UActorComponent::ResolveLoadedReferences()) {
        return false;
    }

    if (!mPendingParentGuid.IsValid()) {
        return true;
    }

    UObject* ResolvedObject{UObjectSystem::Resolve(UObjectSystem::FindHandleByGuid(mPendingParentGuid))};
    if (ResolvedObject == nullptr || !ResolvedObject->GetTypeInfo()->IsA(USceneComponent::StaticTypeInfo())) {
        return false;
    }

    USceneComponent* ParentComponent{static_cast<USceneComponent*>(ResolvedObject)};
    if (!AttachToComponent(ParentComponent)) {
        return false;
    }

    mPendingParentGuid = {};

    return true;
}

void USceneComponent::OnTransformUpdate() {
    OnRenderStateChanged();
}

void USceneComponent::MarkTransformDirty() {
    mWorldTransformDirty = true;
    NotifyTransformUpdate();
}

void USceneComponent::NotifyTransformUpdate() {
    OnTransformUpdate();

    for (const TObjectRef<USceneComponent>& ChildRef : mChildren) {
        if (USceneComponent* Child{ChildRef.Get()}) {
            Child->MarkTransformDirty();
        }
    }
}

void USceneComponent::DrawPanels(IPropertyEditorContext* Context) {
    UActorComponent::DrawPanels(Context);

    if (Context->BeginCategory("Transform")) {
        Context->DrawTransform("Relative Transform", GetRelativeTransform(), [this](const FTransform& Transform) {
            SetRelativeTransform(Transform);
        });
    }

    AActor* Actor{GetOwner()};
    if (Actor == nullptr || !Context->BeginCategory("Attachment")) {
        return;
    }

    if (Actor->GetRootComponent() == this) {
        Context->DrawDisabledText("Root Component");
        return;
    }

    USceneComponent* CurrentParent{GetParent()};
    const char* Preview{CurrentParent != nullptr ? CurrentParent->GetTypeInfo()->mTypeName.data() : "None"};
    std::vector<FPropertyReferenceOption> Candidates{};
    for (const std::unique_ptr<UActorComponent>& Candidate : Actor->GetComponents()) {
        UActorComponent* CandidateComponent{Candidate.get()};
        if (CandidateComponent == nullptr || !CandidateComponent->GetTypeInfo()->IsA<USceneComponent>()) {
            continue;
        }

        auto* Parent{static_cast<USceneComponent*>(CandidateComponent)};
        if (Parent == this)
            continue;

        Candidates.push_back({Parent, FString{Parent->GetTypeInfo()->mTypeName}, Parent == CurrentParent, [this, Parent] {
                                  AttachToComponent(Parent, EAttachmentTransformRule::KeepWorldTransform);
                              }});
    }

    Context->DrawReferencePicker("Parent", Preview, CurrentParent == nullptr, [this] {
        DetachFromComponent(EAttachmentTransformRule::KeepWorldTransform);
    }, Candidates);

    Context->DrawButton("Make Root Component", [this, Actor] {
        DetachFromComponent(EAttachmentTransformRule::KeepWorldTransform);
        Actor->SetRootComponent(this);
    });
}
