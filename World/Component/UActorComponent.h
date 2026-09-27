#pragma once

#include "Core/Base/UObject.h"
#include "Core/Archive/FArchive.h"
#include "Core/Property/IPropertyEditorContext.h"

class AActor;
class UWorld;

class UActorComponent : public UObject {
public:
    UActorComponent() = default;
    ~UActorComponent() override = default;

    UActorComponent(const UActorComponent&) = delete;
    UActorComponent& operator=(const UActorComponent&) = delete;

    UActorComponent(UActorComponent&&) = default;
    UActorComponent& operator=(UActorComponent&&) = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UActorComponent, UObject)

    AActor* GetOwner() const;

    virtual void OnRegister();
    virtual void InitializeComponent();
    virtual void BeginPlay();
    virtual void EndPlay();
    virtual void Tick(float DeltaTime);
    virtual void OnUnregister();
    virtual void DrawPanels(IPropertyEditorContext* Context);

    bool IsActive() const;
    void SetActive(bool BInActive);

    bool IsRegistered() const;
    bool IsInitialized() const;
    bool HasBegunPlay() const;
    UWorld* GetBelongingWorld() const;

    void RegisterComponent(UWorld* World);
    void UnregisterComponent();

    virtual void DestroyComponent(bool BPromoteChildren = false);

    virtual bool ResolveLoadedReferences();

protected:
    void Serialize(FArchive& Archive) override;

private:
    friend class AActor;

    void SetOwner(AActor* InOwner);

private:
    AActor* mOwner{nullptr};
    UWorld* mParentWorld{nullptr};

    bool mBActive{true};
    bool mBRegistered{false};
    bool mBInitialized{false};
    bool mBHasBegunPlay{false};
    bool mBIsBeingDestroyed{false};
};
