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

    virtual void OnUnregister() override;
    void DestroyComponent(bool BPromoteChildren = false) override;
    void DrawPanels(IPropertyEditorContext* Context) override;
    void RemoveChild(USceneComponent* InChild);

    JG_DECLARE_DERIVED_TYPEINFO(USceneComponent, UActorComponent)

    FTransform& GetRelativeTransform();
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

    FTransform GetComponentTransform() const;
    FMatrix GetComponentToWorld() const;
    FVector3 GetComponentLocation() const;
    FRotator GetComponentRotation() const;
    FVector3 GetComponentScale() const;
    USceneComponent* GetParent() const;
    const std::vector<TObjectRef<USceneComponent>>& GetChildren() const;

    // PrimitiveComponent 에서 호출될 world bound 업데이트 위한 함수
    virtual void OnTransformUpdate() {};

protected:
    void Serialize(FArchive& Archive) override;
    bool ResolveLoadedReferences() override;

private:
    void MarkTarnsformDirty();

private:
    FTransform mTransform{};
    FGuid mPendingParentGuid{};

    TObjectRef<USceneComponent> mParent{};
    std::vector<TObjectRef<USceneComponent>> mChildren{};
};
