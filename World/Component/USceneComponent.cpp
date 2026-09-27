#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"

#include "USceneComponent.h"
#include "World/AActor.h"

namespace {
    bool DecomposeWorldTransform(const FMatrix& WorldMatrix, FVector3& OutScale, FQuat& OutRotation, FVector3& OutTranslation) {
        FMatrix TransformMatrix{WorldMatrix};

        const float Row0[3]{TransformMatrix.m_[0][0], TransformMatrix.m_[0][1], TransformMatrix.m_[0][2]};
        const float Row1[3]{TransformMatrix.m_[1][0], TransformMatrix.m_[1][1], TransformMatrix.m_[1][2]};
        const float Row2[3]{TransformMatrix.m_[2][0], TransformMatrix.m_[2][1], TransformMatrix.m_[2][2]};
        for (Uint32 Column{0}; Column < 3; ++Column) {
            TransformMatrix.m_[0][Column] = -Row0[Column];
            TransformMatrix.m_[1][Column] = Row2[Column];
            TransformMatrix.m_[2][Column] = Row1[Column];
        }

        return TransformMatrix.Decompose(OutScale, OutRotation, OutTranslation);
    }

    bool ApplyWorldMatrix(USceneComponent& Component, const FMatrix& DesiredWorld) {
        FVector3 Scale{};
        FQuat Rotation{};
        FVector3 Translation{};
        if (!DecomposeWorldTransform(DesiredWorld, Scale, Rotation, Translation)) {
            return false;
        }

        return Component.SetWorldTransform(FTransform{Translation, Rotation, Scale});
    }
}

FTransform& USceneComponent::GetRelativeTransform() {
    return mTransform;
}

const FTransform& USceneComponent::GetRelativeTransform() const {
    return mTransform;
}

void USceneComponent::SetRelativeTransform(const FTransform& Transform) {
    this->mTransform = Transform;
}

void USceneComponent::SetRelativeLocation(const FVector3& Location) {
    mTransform.SetPosition(Location);
}

void USceneComponent::SetRelativeLocationAndRotation(const FVector3& Location, const FRotator& Rotation) {
    mTransform.SetPosition(Location);
    mTransform.SetRotation(Rotation);
}

FVector3 USceneComponent::GetRelativeLocation() const {
    return mTransform.GetPosition();
}

void USceneComponent::SetRelativeRotation(const FRotator& Rotation) {
    mTransform.SetRotation(Rotation);
}

FRotator USceneComponent::GetRelativeRotation() const {
    return mTransform.GetRotation();
}

void USceneComponent::SetRelativeScale3D(const FVector3& Scale) {
    mTransform.SetScale(Scale);
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
    }
}

void USceneComponent::OnUnregister() {
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
    if (!InChild)
        return;

    std::erase_if(mChildren, [InChild](const TObjectRef<USceneComponent>& ChildRef) {
        return ChildRef.Get() == InChild;
    });

    if (InChild->mParent.Get() == this) {
        InChild->mParent.Reset();
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

        mTransform = RelativeTransform;
        return true;
    }

    mTransform = DesiredWorldTransform;
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

FTransform USceneComponent::GetComponentTransform() const {
    if (USceneComponent * ParentComponent{mParent.Get()}) {
        return mTransform.Compose(ParentComponent->GetComponentTransform());
    }

    return mTransform;
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
    if (ResolvedObject == nullptr ||
        !ResolvedObject->GetTypeInfo()->IsA(USceneComponent::StaticTypeInfo())) {
        return false;
    }

    USceneComponent* ParentComponent{static_cast<USceneComponent*>(ResolvedObject)};
    if (!AttachToComponent(ParentComponent)) {
        return false;
    }

    mPendingParentGuid = {};
    return true;
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
