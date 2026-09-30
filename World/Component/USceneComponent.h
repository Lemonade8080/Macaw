#pragma once

#include "Core/Base/FTransform.h"
#include "Core/Base/TObjectRef.h"
#include "Core/Archive/FArchive.h"
#include "UActorComponent.h"

enum class EAttachmentTransformRule {
    KeepRelativeTransform,
    KeepWorldTransform
};

class USceneComponent : public UActorComponent {
public:
    USceneComponent() = default;
    ~USceneComponent() override = default;

public:
    virtual void OnUnregister() override;
    void DestroyComponent(bool BPromoteChildren = false) override;
    void DrawPanels(IPropertyEditorContext* Context) override;
    void RemoveChild(USceneComponent* InChild);

    JG_DECLARE_DERIVED_TYPEINFO(USceneComponent, UActorComponent)

    const FTransform& GetRelativeTransform() const;
    void SetRelativeTransform(const FTransform& Transform);

    void SetRelativeLocation(const FVector3& Location);
    void SetRelativeLocationAndRotation(const FVector3& Location, const FRotator& Rotation);
    FVector3 GetRelativeLocation() const;

    void SetRelativeRotation(const FRotator& Rotation);
    FRotator GetRelativeRotation() const;

    void SetRelativeScale3D(const FVector3& Scale);
    FVector3 GetRelativeScale3D() const;

    bool AttachToComponent(USceneComponent* Parent, EAttachmentTransformRule Rule = EAttachmentTransformRule::KeepRelativeTransform);
    bool DetachFromComponent(EAttachmentTransformRule Rule = EAttachmentTransformRule::KeepRelativeTransform);

    bool SetWorldTransform(const FTransform& WorldTransform);
    bool SetWorldTransform(const FMatrix& WorldTransform);
    bool SetWorldLocation(const FVector3& Location);
    bool SetWorldLocationAndRotation(const FVector3& Location, const FRotator& Rotation);
    bool SetWorldRotation(const FRotator& Rotation);
    bool SetWorldScale3D(const FVector3& Scale);

    const FTransform& GetComponentTransform() const;
    Uint64 GetTransformRevision() const;
    FMatrix GetComponentToWorld() const;
    FVector3 GetComponentLocation() const;
    FRotator GetComponentRotation() const;
    FVector3 GetComponentScale() const;

    USceneComponent* GetParent() const;
    const std::vector<TObjectRef<USceneComponent>>& GetChildren() const;

    // PrimitiveComponent 에서 호출될 world bound 업데이트 위한 함수
    virtual void OnTransformUpdate();

    void Serialize(FArchive& Archive) override;
    bool ResolveLoadedReferences() override;

private:
    void MarkTransformDirty();
    void NotifyTransformUpdate();

private:
    FTransform mTransform{};
    mutable FTransform mWorldTransform{};
    mutable FObjectHandle mCachedParentHandle{};
    mutable Uint64 mCachedParentRevision{};
    mutable Uint64 mTransformRevision{};
    mutable bool mWorldTransformDirty{true};
    FGuid mPendingParentGuid{};

    TObjectRef<USceneComponent> mParent{};
    std::vector<TObjectRef<USceneComponent>> mChildren{};
};
