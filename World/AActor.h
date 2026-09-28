#pragma once
#pragma once

#include "Core/Common.h"

#include "Core/Base/UObject.h"
#include "World/Component/UActorComponent.h"
#include "World/Component/USceneComponent.h"

class UWorld;

/// <summary>
/// World에 소속되며 UActorComponent를 RAII 방식으로 소유하는 게임플레이 객체입니다.
/// Actor의 월드 Transform은 RootComponent의 ComponentToWorld Transform으로 정의됩니다.
/// </summary>
class AActor : public UObject {
public:
    /// <summary>새 Actor를 생성합니다. World 소속과 Component 등록은 아직 수행하지 않습니다.</summary>
    AActor() = default;
    /// <summary>World 및 소유 Component와의 수명 관계를 정리합니다.</summary>
    ~AActor() override;

    AActor(const AActor&) = delete;
    AActor& operator=(const AActor&) = delete;

    AActor(AActor&&) = default;
    AActor& operator=(AActor&&) = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(AActor, UObject)

    /// <summary>Actor가 소유하는 새 Component를 생성하고 추가합니다.</summary>
    /// <typeparam name="T">생성할 UActorComponent 파생 타입입니다.</typeparam>
    /// <returns>Actor가 소유하는 새 Component입니다.</returns>
    template <typename T>
        requires std::is_base_of_v<UActorComponent, T>
    T* AddComponent();

    UActorComponent* AddComponent(const FTypeInfo& Type);

    /// <summary>지정한 타입과 호환되는 첫 번째 소유 Component를 찾습니다.</summary>
    /// <typeparam name="T">찾을 UActorComponent 파생 타입입니다.</typeparam>
    /// <returns>찾은 Component 또는 없으면 nullptr입니다.</returns>
    template <typename T>
        requires std::is_base_of_v<UActorComponent, T>
    T* GetComponent();

    /// <summary>Actor가 RAII 방식으로 소유하는 모든 Component를 반환합니다.</summary>
    const std::vector<std::unique_ptr<UActorComponent>>& GetComponents() const;

    /// <summary>Actor의 RootComponent를 반환합니다.</summary>
    USceneComponent* GetRootComponent();
    /// <summary>Actor의 RootComponent를 읽기 전용으로 반환합니다.</summary>
    const USceneComponent* GetRootComponent() const;

    /// <summary>Actor를 World에 연결하거나 nullptr로 제거합니다. 연결 중 Component 수명 주기도 진행됩니다.</summary>
    /// <param name="InWorld">새 소속 World 또는 제거를 위한 nullptr입니다.</param>
    void SetWorld(UWorld* InWorld);
    /// <summary>현재 소속된 World를 반환합니다.</summary>
    UWorld* GetWorld() const;
    /// <summary>소속 World에 Actor의 지연 파괴를 요청합니다.</summary>
    /// <returns>파괴 요청이 수락되었으면 true입니다.</returns>
    bool Destroy();
    /// <summary>Actor가 소유한 SceneComponent를 RootComponent로 지정합니다.</summary>
    /// <param name="InRootComponent">새 RootComponent 또는 nullptr입니다.</param>
    /// <returns>Component가 Actor 소유이거나 nullptr이면 true입니다.</returns>
    bool SetRootComponent(USceneComponent* InRootComponent);

    /// <summary>RootComponent 기준의 Actor 월드 Transform을 반환합니다.</summary>
    FTransform GetActorTransform() const;
    /// <summary>RootComponent를 통해 Actor 월드 Transform을 설정합니다.</summary>
    /// <param name="Transform">설정할 월드 Transform입니다.</param>
    /// <returns>RootComponent가 있어 Transform을 설정했으면 true입니다.</returns>
    bool SetActorTransform(const FTransform& Transform);
    /// <summary>Actor의 월드 위치를 반환합니다.</summary>
    FVector3 GetActorLocation() const;
    /// <summary>Actor의 월드 위치를 설정합니다.</summary>
    /// <param name="Location">설정할 월드 위치입니다.</param>
    /// <returns>RootComponent가 있어 위치를 설정했으면 true입니다.</returns>
    bool SetActorLocation(const FVector3& Location);
    /// <summary>Actor의 월드 위치와 회전을 한 번에 설정합니다.</summary>
    /// <param name="Location">설정할 월드 위치입니다.</param>
    /// <param name="Rotation">설정할 월드 회전입니다.</param>
    /// <returns>RootComponent가 있어 Transform을 설정했으면 true입니다.</returns>
    bool SetActorLocationAndRotation(const FVector3& Location, const FRotator& Rotation);
    /// <summary>Actor의 월드 회전을 반환합니다.</summary>
    FRotator GetActorRotation() const;
    /// <summary>Actor의 월드 회전을 설정합니다.</summary>
    /// <param name="Rotation">설정할 월드 회전입니다.</param>
    /// <returns>RootComponent가 있어 회전을 설정했으면 true입니다.</returns>
    bool SetActorRotation(const FRotator& Rotation);
    /// <summary>Actor의 월드 스케일을 반환합니다.</summary>
    FVector3 GetActorScale3D() const;
    /// <summary>Actor의 월드 스케일을 설정합니다.</summary>
    /// <param name="Scale">설정할 월드 스케일입니다.</param>
    /// <returns>RootComponent가 있어 스케일을 설정했으면 true입니다.</returns>
    bool SetActorScale3D(const FVector3& Scale);

    /// <summary>RootComponent 기준의 Actor 상대 Transform을 반환합니다.</summary>
    FTransform GetActorRelativeTransform() const;
    /// <summary>RootComponent 기준의 Actor 상대 Transform을 설정합니다.</summary>
    /// <param name="Transform">설정할 부모 기준 Transform입니다.</param>
    /// <returns>RootComponent가 있어 Transform을 설정했으면 true입니다.</returns>
    bool SetActorRelativeTransform(const FTransform& Transform);
    /// <summary>Actor의 부모 기준 상대 위치를 반환합니다.</summary>
    FVector3 GetActorRelativeLocation() const;
    /// <summary>Actor의 부모 기준 상대 위치를 설정합니다.</summary>
    /// <param name="Location">설정할 부모 기준 위치입니다.</param>
    /// <returns>RootComponent가 있어 위치를 설정했으면 true입니다.</returns>
    bool SetActorRelativeLocation(const FVector3& Location);
    /// <summary>Actor의 부모 기준 상대 위치와 회전을 한 번에 설정합니다.</summary>
    /// <param name="Location">설정할 부모 기준 위치입니다.</param>
    /// <param name="Rotation">설정할 부모 기준 회전입니다.</param>
    /// <returns>RootComponent가 있어 Transform을 설정했으면 true입니다.</returns>
    bool SetActorRelativeLocationAndRotation(const FVector3& Location, const FRotator& Rotation);
    /// <summary>Actor의 부모 기준 상대 회전을 반환합니다.</summary>
    FRotator GetActorRelativeRotation() const;
    /// <summary>Actor의 부모 기준 상대 회전을 설정합니다.</summary>
    /// <param name="Rotation">설정할 부모 기준 회전입니다.</param>
    /// <returns>RootComponent가 있어 회전을 설정했으면 true입니다.</returns>
    bool SetActorRelativeRotation(const FRotator& Rotation);
    /// <summary>Actor의 부모 기준 상대 스케일을 반환합니다.</summary>
    FVector3 GetActorRelativeScale3D() const;
    /// <summary>Actor의 부모 기준 상대 스케일을 설정합니다.</summary>
    /// <param name="Scale">설정할 부모 기준 스케일입니다.</param>
    /// <returns>RootComponent가 있어 스케일을 설정했으면 true입니다.</returns>
    bool SetActorRelativeScale3D(const FVector3& Scale);

    /// <summary>Actor가 BeginPlay 수명 주기를 완료했는지 반환합니다.</summary>
    bool HasBegunPlay() const;
    /// <summary>활성 Component에 프레임 Tick을 전달합니다.</summary>
    /// <param name="DeltaTime">이전 프레임 이후 경과 시간입니다.</param>
    virtual void Tick(float DeltaTime);

    /// <summary>직렬화 데이터에서 Component 인스턴스와 Object 등록 정보를 먼저 생성합니다.</summary>
    /// <param name="Archive">로드 중인 아카이브입니다.</param>
    /// <returns>모든 Component를 만들고 등록했으면 true입니다.</returns>
    bool PreLoadComponents(FArchive& Archive);
    /// <summary>Component와 RootComponent의 지연 참조를 해석합니다.</summary>
    /// <returns>모든 참조를 해석했으면 true입니다.</returns>
    bool ResolveLoadedReferences();

protected:
    /// <summary>Actor가 World에 추가된 직후 호출됩니다.</summary>
    virtual void OnAddedToWorld();
    /// <summary>등록된 Component를 초기화할 때 호출됩니다.</summary>
    virtual void InitializeComponents();
    /// <summary>Actor와 Component의 플레이 시작 시점에 호출됩니다.</summary>
    virtual void BeginPlay();
    /// <summary>Actor와 Component의 플레이 종료 시점에 호출됩니다.</summary>
    virtual void EndPlay();
    /// <summary>Actor가 World에서 제거되기 전에 호출됩니다.</summary>
    virtual void OnRemovedFromWorld();

    void Serialize(FArchive& Archive) override;

private:
    friend class UActorComponent;

    void RemoveOwnedComponent(UActorComponent* Component);

    std::vector<std::unique_ptr<UActorComponent>> mComponents{};
    USceneComponent* mRootComponent{nullptr};

    FGuid mPendingRootComponentGuid{};

    UWorld* mWorld{nullptr};
    bool mBHasBegunPlay{false};
};

template <typename T> requires std::is_base_of_v<UActorComponent, T> T* AActor::AddComponent() {
    std::unique_ptr<T> NewComponent{std::make_unique<T>()};
    T* ComponentPtr{NewComponent.get()};

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

template <typename T> requires std::is_base_of_v<UActorComponent, T> T* AActor::GetComponent() {
    for (const auto& Component : mComponents) {
        if (Component->GetTypeInfo()->IsA(T::StaticTypeInfo())) {
            return static_cast<T*>(Component.get());
        }
    }

    return nullptr;
}
