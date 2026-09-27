#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "AActor.h"
#include "Component/UCameraComponent.h"
#include "Component/UStaticMeshComponent.h"
#include "Component/UCollisionComponent.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/IAssetRegistryMutator.h"
#include "Asset/UMesh.h"
#include "Core/Base/TObjectRef.h"

#include "Core/Common.h"
#include "Core/Base/UObject.h"
#include "Core/Base/UObjectSystem.h"
#include "Core/Base/FRenderProbe.h"
#include "FWorldEditorContext.h"
#include "FWorldTime.h"
#include "Core/Channel/Messages/FMousePickRequestMessage.h"
#include "Core/Channel/FEditorInfo.h"
#include "World/Subsystem/UCameraSubsystem.h"
#include "World/Subsystem/UCollisionSubsystem.h"
#include "World/Subsystem/UPickingSubsystem.h"
#include "World/Subsystem/URenderSubsystem.h"
#include "World/Subsystem/UBillboardSubsystem.h"
#include "World/Subsystem/UTextSubsystem.h"
#include "World/Subsystem/ULightSubsystem.h"
#include "Core/Channel/Messages/FKeyboardCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraRotateRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraDollyRequestMessage.h"

class UWorld : public UObject {
public:
    UWorld();
    ~UWorld() override;

public:
    AActor* AddActor(std::unique_ptr<AActor> InActor);

    template <typename T> requires std::is_base_of_v<AActor, T> T* AdoptActor();

    AActor* SpawnActor(const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const FVector3& Position);
    bool DestroyActor(AActor* Actor);
    void FlushPendingDestroyActors();

    const TArray<std::unique_ptr<AActor>>& GetActors() const;
    FRenderProbe& BuildRenderProbe();

    void SetEditorContext(FWorldEditorContext* InEditorContext);
    FWorldEditorContext* GetEditorContext() const noexcept;

    void Tick(float DeltaTime);
    FWorldTime& GetTime();
    const FWorldTime& GetTime() const;

    URenderSubsystem& GetRenderSubsystem();
    const URenderSubsystem& GetRenderSubsystem() const;
    UCollisionSubsystem& GetCollisionSubsystem();
    const UCollisionSubsystem& GetCollisionSubsystem() const;
    UPickingSubsystem& GetPickingSubsystem();
    const UPickingSubsystem& GetPickingSubsystem() const;
    UCameraSubsystem& GetCameraSubsystem();
    const UCameraSubsystem& GetCameraSubsystem() const;
    UBillboardSubsystem& GetBillboardSubsystem();
    const UBillboardSubsystem& GetBillboardSubsystem() const;

    UTextSubsystem& GetTextSubsystem();
    const UTextSubsystem& GetTextSubsystem() const;
    ULightSubsystem& GetLightSubsystem();
    const ULightSubsystem& GetLightSubsystem() const;

    bool SaveScene(const FString& SceneName, const IAssetRegistry* AssetRegistry);
    bool LoadScene(const std::filesystem::path& ScenePath);

    JG_DECLARE_DERIVED_TYPEINFO(UWorld, UObject);

    void HandleMousePickRequest(const FMousePickRequestMessage& Message);
    void HandleSpawnComponent(const FMessageSpawnComponent& Message, const IAssetRegistry* AssetRegistry);
#ifdef OBJ_VIEWER
    void HandleMouseCameraRotateRequest(const FMouseCameraRotateRequestMessage& Message);
    void HandleKeyboardCameraMoveRequest(const FKeyboardCameraMoveRequestMessage& Message);
    void HandleMouseCameraMoveRequestMessage(const FMouseCameraMoveRequestMessage& Message);
    void HandleMouseCameraDollyRequestMessage(const FMouseCameraDollyRequestMessage& Message);
#endif
    void SetAssetRegistry(const IAssetRegistry* InAssetRegistry, IAssetRegistryMutator* InAssetRegistryMutator = nullptr);

    const IAssetRegistry* GetAssetRegistry() const;
    IAssetRegistryMutator* GetAssetRegistryMutator() const;

    FName MakeUniqueObjectName(std::string_view SourceName);
    AActor* FindActorByName(FName InName) const;

private:
    void ClearActors();
    void InitializeSubsystems();
    void DeinitializeSubsystems();

private:
    FWorldTime mTime{};
    TArray<std::unique_ptr<AActor>> mActors{};
    TArray<AActor*> mPendingDestroyActors{};

    TArray<UStaticMeshComponent*> mRenderableComponents{};
    TArray<TObjectRef<UCollisionComponent>> mCollisionComponents{};

    FWorldEditorContext* mEditorContext{nullptr};
    const IAssetRegistry* mAssetRegistry{nullptr};
    IAssetRegistryMutator* mAssetRegistryMutator{nullptr};

    std::unique_ptr<URenderSubsystem> mRenderSubsystem{};
    std::unique_ptr<UCollisionSubsystem> mCollisionSubsystem{};
    std::unique_ptr<UPickingSubsystem> mPickingSubsystem{};
    std::unique_ptr<UCameraSubsystem> mCameraSubsystem{};

    std::unique_ptr<UBillboardSubsystem> mBillboardSubsystem{};
    std::unique_ptr<UTextSubsystem> mTextSubsystem{};
    std::unique_ptr<ULightSubsystem> mLightSubsystem{};

    FRenderProbe mProbe{};
};

template <typename T> requires std::is_base_of_v<AActor, T> T* UWorld::AdoptActor() {
    std::unique_ptr<T> NewActor{std::make_unique<T>()};

    T* ActorPtr{NewActor.get()};

    if (AddActor(std::move(NewActor)) == nullptr) {
        return nullptr;
    }

    ActorPtr->SetName(MakeUniqueObjectName(ActorPtr->GetTypeInfo()->mTypeName));

    return ActorPtr;
}
