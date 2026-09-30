#include "pch.h"
#include "UWorld.h"
#include "FTemporarySceneLoader.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "AActor.h"
#include "Component/UCameraComponent.h"
#include "Component/UActorComponent.h"
#include "Component/USceneComponent.h"
#include "Component/UStaticMeshComponent.h"
#include "Subsystem/UCameraSubsystem.h"
#include "Subsystem/UCollisionSubsystem.h"
#include "Subsystem/UPickingSubsystem.h"
#include "Subsystem/URenderSubsystem.h"
#include "Subsystem/UTextSubsystem.h"
#include "Subsystem/UBillboardSubsystem.h"
#include "Subsystem/ULightSubsystem.h"
#include "Component/UCollisionComponent.h"
#include "Component/UBillboardTextComponent.h"
#include "Component/UBillboardComponent.h"
#include "Component/ULightComponent.h"
#include "Core/Channel/Messages/FMousePickRequestMessage.h"
#include "FWorldEditorContext.h"
#ifdef OBJ_VIEWER
#include "Core/Channel/Messages/FKeyboardCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraDollyRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraRotateRequestMessage.h"
#endif
#include "Core/Channel/FEditorInfo.h"
#include "Asset/Pipeline/UPipeline.h"
#include "Asset/UMesh.h"
#include "Asset/FAssetRegistry.h"

#include "Serialization/FArchiveJson.h"
#include "../Core/Base/TypeRegistry.h"
#include "../Core/Base/UObjectSystem.h"
#include "Core/Asset/IAssetRegistry.h"
#include "../Core/Console/Console.h"

#include <filesystem>
#include <fstream>
#include <ranges>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include "Component/UNameTagComponent.h"

UWorld::UWorld() {
    InitializeSubsystems();
}

UWorld::~UWorld() {
    for (const std::unique_ptr<AActor>& Actor : mActors) {
        Actor->SetWorld(nullptr);
        UObjectSystem::Unregister(Actor.get(), Actor->GetHandle());
    }

    mActors.clear();
    DeinitializeSubsystems();
}

AActor* UWorld::SpawnActor(const FAssetHandle& MeshHandle, const FAssetHandle& PipelineHandle, const FAssetHandle& MaterialHandle, const FVector3& Position) {
    if (mAssetRegistry == nullptr || mAssetRegistry->ResolveAsset<UMesh>(MeshHandle) == nullptr) {
        return nullptr;
    }

    AActor* Actor{UWorld::AdoptActor<AActor>()};
    if (Actor == nullptr) {
        return nullptr;
    }

    UStaticMeshComponent* MeshComponent{Actor->AddComponent<UStaticMeshComponent>()};
    if (MeshComponent == nullptr) {
        DestroyActor(Actor);
        return nullptr;
    }
    Actor->SetRootComponent(MeshComponent);

    MeshComponent->SetMeshHandle(MeshHandle);
    MeshComponent->SetPipelineHandle(PipelineHandle);
    MeshComponent->SetMaterialHandle(MaterialHandle);

    MeshComponent->SetRelativeLocation(FVector3{ Position.mX, Position.mY, Position.mZ});

    UNameTagComponent* NameTagComponent{Actor->AddComponent<UNameTagComponent>()};
    NameTagComponent->AttachToComponent(MeshComponent);
    NameTagComponent->SetTargetActor(nullptr);
    NameTagComponent->SetTargetLocalOffset(NameTagComponent->GetTargetLocalOffset());
    NameTagComponent->SetVisible(true);
    NameTagComponent->SetActive(false);
    if (mAssetRegistry != nullptr) {
        NameTagComponent->SetPipelineHandle(mAssetRegistry->FindAsset(FAssetPath{"/Game/Pipeline/Text.json"}));
        NameTagComponent->SetFontHandle(mAssetRegistry->FindAsset(FAssetPath{"/Game/Font/NotoSansKR-Medium.ttf"}));
    }

    return Actor;
}

bool UWorld::DestroyActor(AActor* Actor) {
    if (Actor == nullptr) {
        return false;
    }

    auto It{std::ranges::find_if(mActors, [Actor](const std::unique_ptr<AActor>& Ptr) {
        return Ptr.get() == Actor;
    })};

    if (It == mActors.end()) {
        return false;
    }

    if (std::ranges::find(mPendingDestroyActors, Actor) != mPendingDestroyActors.end()) {
        return true;
    }

    mPendingDestroyActors.push_back(Actor);
    return true;
}

void UWorld::FlushPendingDestroyActors() 
{
    bool bRemovedAnyActor = false;

    for (AActor* Actor : mPendingDestroyActors) 
    {
        if (Actor == nullptr) {continue;}

        auto It{std::ranges::find_if(mActors, [Actor](const std::unique_ptr<AActor>& Ptr) {
            return Ptr.get() == Actor;
        })};

        if (It == mActors.end()) { continue; }

        if (mEditorContext != nullptr && mEditorContext->GetSelectedActor() == Actor) {
            mEditorContext->ClearSelection();
        }

        Actor->SetWorld(nullptr);
        UObjectSystem::Unregister(Actor, Actor->GetHandle());

        mActors.erase(It);
        bRemovedAnyActor = true;
    }

    mPendingDestroyActors.clear();
    if (bRemovedAnyActor) { MarkOutlinerDirty(); }
}

void UWorld::AttachActor(AActor* Child, AActor* Parent)
{
    if (Child == nullptr || Parent == nullptr || Child == Parent)
    {
        return;
    }

    USceneComponent* ChildRoot = Child->GetRootComponent();
    USceneComponent* ParentRoot = Parent->GetRootComponent();

    if (ChildRoot == nullptr || ParentRoot == nullptr)
    {
        return;
    }

    // 이미 같은 부모라면 변경 없음
    if (ChildRoot->GetParent() == ParentRoot)
    {
        return;
    }

    ChildRoot->AttachToComponent(ParentRoot);
    MarkOutlinerDirty();
}

void UWorld::DetachActor(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    USceneComponent* RootComponent = Actor->GetRootComponent();

    if (RootComponent == nullptr)
    {
        return;
    }

    if (RootComponent->DetachFromComponent(EAttachmentTransformRule::KeepWorldTransform))
    {
        MarkOutlinerDirty();
    }
}

bool UWorld::RenameActor(AActor* Actor, const FName& NewName)
{
    if (Actor == nullptr)
    {
        return false;
    }

    if (Actor->GetWorld() != this)
    {
        return false;
    }

    if (Actor->GetName() == NewName)
    {
        return false;
    }

    Actor->SetName(NewName);

    MarkOutlinerDirty();

    return true;
}

const TArray<std::unique_ptr<AActor>>& UWorld::GetActors() const {
    return mActors;
}

void UWorld::InitializeSubsystems() {
    mRenderSubsystem = std::make_unique<URenderSubsystem>();
    mCollisionSubsystem = std::make_unique<UCollisionSubsystem>();
    mPickingSubsystem = std::make_unique<UPickingSubsystem>();
    mCameraSubsystem = std::make_unique<UCameraSubsystem>();
    mTextSubsystem = std::make_unique<UTextSubsystem>();
    mBillboardSubsystem = std::make_unique<UBillboardSubsystem>();
    mLightSubsystem = std::make_unique<ULightSubsystem>();

    mRenderSubsystem->Initialize(this);
    mCollisionSubsystem->Initialize(this);
    mPickingSubsystem->Initialize(this);
    mCameraSubsystem->Initialize(this);
    mTextSubsystem->Initialize(this);
    mLightSubsystem->Initialize(this);

    mBillboardSubsystem->Initialize(this);
}

void UWorld::DeinitializeSubsystems() {
    if (mCameraSubsystem != nullptr) {
        mCameraSubsystem->Deinitialize();
    }
    if (mCollisionSubsystem != nullptr) {
        mCollisionSubsystem->Deinitialize();
    }
    if (mPickingSubsystem != nullptr) {
        mPickingSubsystem->Deinitialize();
    }
    if (mRenderSubsystem != nullptr) {
        mRenderSubsystem->Deinitialize();
    }
    if (mTextSubsystem != nullptr) {
        mTextSubsystem->Deinitialize();
    }
    if (mBillboardSubsystem != nullptr) {
        mBillboardSubsystem->Deinitialize();
    }
    if (mLightSubsystem != nullptr) {
        mLightSubsystem->Deinitialize();
    }
}

UTextSubsystem& UWorld::GetTextSubsystem() {
    return *mTextSubsystem;
}

const UTextSubsystem& UWorld::GetTextSubsystem() const {
    return *mTextSubsystem;
}

ULightSubsystem& UWorld::GetLightSubsystem() {
    return *mLightSubsystem;
}

const ULightSubsystem& UWorld::GetLightSubsystem() const {
    return *mLightSubsystem;
}

void UWorld::BuildSceneRenderData(FSceneRenderData& Scene) {
    mRenderSubsystem->BuildRenderProbes(Scene);

    mLightSubsystem->BuildLightProbes(Scene);
    mTextSubsystem->BuildTextProbes(Scene);
    mBillboardSubsystem->BuildRenderProbes(Scene);
}

void UWorld::MarkOutlinerDirty() {
    ++mOutlinerRevision;
}

uint64 UWorld::GetOutlinerRevision() const {
    return mOutlinerRevision;
}

void UWorld::SetEditorContext(FWorldEditorContext* InEditorContext) {
    if (InEditorContext == mEditorContext) {
        return;
    }

    mEditorContext = InEditorContext;
}

FWorldEditorContext* UWorld::GetEditorContext() const noexcept {
    return mEditorContext;
}

void UWorld::Tick(float DeltaTime) {
    mTime.Tick(static_cast<double>(DeltaTime));
    const float WorldDeltaTime{static_cast<float>(mTime.GetDeltaSeconds())};
    if (WorldDeltaTime > 0.0f) {
        for (const std::unique_ptr<AActor>& Actor : mActors) {
            Actor->Tick(WorldDeltaTime);
        }
    }

    FlushPendingDestroyActors();
}

FWorldTime& UWorld::GetTime() {
    return mTime;
}

const FWorldTime& UWorld::GetTime() const {
    return mTime;
}

URenderSubsystem& UWorld::GetRenderSubsystem() {
    return *mRenderSubsystem;
}

const URenderSubsystem& UWorld::GetRenderSubsystem() const {
    return *mRenderSubsystem;
}

UCollisionSubsystem& UWorld::GetCollisionSubsystem() {
    return *mCollisionSubsystem;
}

const UCollisionSubsystem& UWorld::GetCollisionSubsystem() const {
    return *mCollisionSubsystem;
}

UPickingSubsystem& UWorld::GetPickingSubsystem() {
    return *mPickingSubsystem;
}

const UPickingSubsystem& UWorld::GetPickingSubsystem() const {
    return *mPickingSubsystem;
}

UCameraSubsystem& UWorld::GetCameraSubsystem() {
    return *mCameraSubsystem;
}

const UCameraSubsystem& UWorld::GetCameraSubsystem() const {
    return *mCameraSubsystem;
}

UBillboardSubsystem& UWorld::GetBillboardSubsystem() {
    return *mBillboardSubsystem;
}

const UBillboardSubsystem& UWorld::GetBillboardSubsystem() const {
    return *mBillboardSubsystem;
}

bool UWorld::SaveScene(const FString& SceneName, const IAssetRegistry* AssetRegistry) {
    std::filesystem::path CurrentPath{std::filesystem::current_path()};
    std::filesystem::path SceneDir{CurrentPath / "scenes"};
    if (!std::filesystem::exists(SceneDir))
        std::filesystem::create_directories(SceneDir);
    std::filesystem::path FilePath{SceneDir / (SceneName.c_str() + std::string(".json"))};

    rapidjson::Document Document{};
    Document.SetObject();
    rapidjson::Document::AllocatorType& Allocator{Document.GetAllocator()};

    FArchiveJson ArchiveSave{Document, Allocator};
    ArchiveSave.SetAssetRegistry(AssetRegistry);

    Uint32 FormatVersion{2};
    ArchiveSave.Serialize("FormatVersion", FormatVersion);

    std::size_t ArraySize{static_cast<std::size_t>(mActors.size())};
    ArchiveSave.BeginArrayScope("Actors", ArraySize);
    for (std::size_t CurrentIndex{0}, EndIndex{mActors.size()}; CurrentIndex < EndIndex; ++CurrentIndex) {
        ArchiveSave.BeginObjectScope(std::to_string(CurrentIndex));
        mActors[CurrentIndex]->Save(ArchiveSave);
        ArchiveSave.EndObjectScope();
    }
    ArchiveSave.EndArrayScope();

    std::ofstream OutputFileStream{FilePath};
    if (!OutputFileStream.is_open())
        return false;

    rapidjson::OStreamWrapper StreamWrapper{OutputFileStream};
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> Writer{StreamWrapper};
    Document.Accept(Writer);
    OutputFileStream.close();

    return true;
}

bool UWorld::LoadScene(const std::filesystem::path& ScenePath) {
    if (ScenePath.extension() == ".scene") {
        FAssetRegistry* Registry{dynamic_cast<FAssetRegistry*>(mAssetRegistryMutator)};
        if (Registry == nullptr) {
            return false;
        }

        FTemporarySceneLoader Loader{};
        const bool Loaded{Loader.Load(ScenePath, *this, *Registry)};
        if (!Loaded) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to load temporary scene: %s", ScenePath.generic_string().c_str());
        }
        return Loaded;
    }

    std::ifstream InputFileStream{ScenePath};
    if (!InputFileStream.is_open()) {
        return false;
    }

    std::stringstream Buffer{};
    Buffer << InputFileStream.rdbuf();
    std::string LoadedJsonString{Buffer.str()};
    InputFileStream.close();

    rapidjson::Document LoadDocument{};
    LoadDocument.Parse(LoadedJsonString.c_str());

    if (LoadDocument.HasParseError() ||
        !LoadDocument.IsObject() ||
        !LoadDocument.HasMember("FormatVersion") ||
        !LoadDocument["FormatVersion"].IsUint() ||
        LoadDocument["FormatVersion"].GetUint() != 2 ||
        !LoadDocument.HasMember("Actors") ||
        !LoadDocument["Actors"].IsArray()) {
        return false;
    }

    if (mAssetRegistry == nullptr) {
        return false;
    }

    ClearActors();

    const auto FailLoad{[this]() {
        ClearActors();
        return false;
    }};

    for (rapidjson::Value& ActorJson : LoadDocument["Actors"].GetArray()) {
        if (!ActorJson.IsObject() ||
            !ActorJson.HasMember("Guid") || !ActorJson["Guid"].IsString() ||
            !ActorJson.HasMember("TypeName") || !ActorJson["TypeName"].IsString()) {
            return FailLoad();
        }

        FGuid ActorGuid{};
        if (!ActorGuid.Parse(ActorJson["Guid"].GetString())) {
            return FailLoad();
        }

        FString TypeName{ActorJson["TypeName"].GetString()};
        const FTypeInfo* Type{TypeRegistry::Find(TypeName)};
        if (Type == nullptr || Type->mCreator == nullptr) {
            return FailLoad();
        }

        std::unique_ptr<UObject> CreatedObject{Type->mCreator()};
        if (CreatedObject == nullptr ||
            !CreatedObject->GetTypeInfo()->IsA(AActor::StaticTypeInfo())) {
            return FailLoad();
        }

        std::unique_ptr<AActor> ActorPtr{static_cast<AActor*>(CreatedObject.release())};
        UObjectSystem::RegisterWithGuid(ActorPtr.get(), ActorGuid);

        FArchiveJson ArchiveLoad{ActorJson};
        if (!ActorPtr->PreLoadComponents(ArchiveLoad)) {
            UObjectSystem::Unregister(ActorPtr.get(), ActorPtr->GetHandle());
            return FailLoad();
        }

        mActors.emplace_back(std::move(ActorPtr));
    }

    for (std::size_t ActorIndex{0}; ActorIndex < mActors.size(); ++ActorIndex) {
        rapidjson::Value& ActorJson{LoadDocument["Actors"][static_cast<rapidjson::SizeType>(ActorIndex)]};
        FArchiveJson ArchiveLoad{ActorJson};
        ArchiveLoad.SetAssetRegistry(mAssetRegistry);
        mActors[ActorIndex]->Load(ArchiveLoad);
    }

    for (const std::unique_ptr<AActor>& Actor : mActors) {
        if (!Actor->ResolveLoadedReferences()) {
            return FailLoad();
        }
    }

    for (const std::unique_ptr<AActor>& Actor : mActors) {
        Actor->SetWorld(this);
    }

    return true;
}

void UWorld::HandleMousePickRequest(const FMousePickRequestMessage& Message) {
    if (Message.mViewportWidth != 0 && Message.mViewportHeight != 0) {
        const float NdcX{(2.0f * (static_cast<float>(Message.mScreenX) - static_cast<float>(Message.mViewportLeft)) / static_cast<float>(Message.mViewportWidth)) - 1.0f};
        const float NdcY{1.0f - (2.0f * (static_cast<float>(Message.mScreenY) - static_cast<float>(Message.mViewportTop)) / static_cast<float>(Message.mViewportHeight))};

        FMatrix InverseViewProjection{};
        if (!Message.mViewProjection.TryInverse(InverseViewProjection))
            return;
        FVector3 RayOrigin{}, RayEnd{};
        if (!InverseViewProjection.TransformCoord({NdcX, NdcY, 0.0f}, RayOrigin) || !InverseViewProjection.TransformCoord({NdcX, NdcY, 1.0f}, RayEnd))
            return;
        FVector3 RayDirection{RayEnd - RayOrigin};

        if (RayDirection.LengthSquared() > 0.0f) {
            RayDirection.Normalize();

            UPrimitiveComponent* NearestPrimitive{nullptr};
            float NearestDistance{0.0f};
            FMatrix CameraWorld{};
            if (!Message.mView.TryInverse(CameraWorld))
                return;
            if (GetPickingSubsystem().Raycast(FRay{RayOrigin.ToSimpleMath(), RayDirection.ToSimpleMath()}, NearestPrimitive, NearestDistance, &CameraWorld)) {
                Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Raycast hit primitive component %f", NearestDistance);
            }

            AActor* PreviousActor{mEditorContext != nullptr ? mEditorContext->GetSelectedActor() : nullptr};
            AActor* SelectedActor{NearestPrimitive != nullptr ? NearestPrimitive->GetOwner() : nullptr};

            if (PreviousActor != nullptr && PreviousActor != SelectedActor) {
                if (UNameTagComponent * NameTag{PreviousActor->GetComponent<UNameTagComponent>()}) {
                    NameTag->SetActive(false);
                }
            }
            if (SelectedActor != nullptr) {
                if (mEditorContext != nullptr) {
                    mEditorContext->SetSelectedComponent(NearestPrimitive);
                }

                if (UNameTagComponent * NameTag{SelectedActor->GetComponent<UNameTagComponent>()}) {
                    NameTag->SetActive(true);
                }
            } else if (mEditorContext != nullptr) {
                mEditorContext->ClearSelection();
            }
        }
    }
}

#ifdef OBJ_VIEWER
void UWorld::HandleMouseCameraRotateRequest(const FMouseCameraRotateRequestMessage& Message) {
    UCameraComponent* Camera{GetCameraSubsystem().GetMainCamera()};
    if (Camera == nullptr) {
        return;
    }

    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};
    const float RotationSensitivity{Settings.mRotationSensitivity * 0.001f};
    constexpr float MaximumForwardUp{0.99f};
    FTransform CameraTransform{Camera->GetRelativeTransform()};
    FVector3 Forward{CameraTransform.ToMatrixNoScale().Forward()};
    Forward.Normalize();

    const float CurrentYaw{std::atan2(Forward.mY, Forward.mX)};
    const float CurrentElevation{std::asin(std::clamp(Forward.mZ, -1.0f, 1.0f))};
    const float MaximumElevation{std::asin(MaximumForwardUp)};
    const float NewYaw{CurrentYaw + Message.DeltaX * RotationSensitivity};
    const float NewElevation{std::clamp(CurrentElevation + Message.DeltaY * RotationSensitivity, -MaximumElevation, MaximumElevation)};

    const FQuat YawRotation{FQuat::CreateFromAxisAngle(FVector3::UnitZ, NewYaw)};
    const FQuat PitchRotation{FQuat::CreateFromAxisAngle(FVector3::UnitY, -NewElevation)};
    FQuat NewRotation{FQuat::Concatenate(YawRotation, PitchRotation)};
    NewRotation.Normalize();
    CameraTransform.SetRotation(NewRotation);

    Camera->SetRelativeTransform(CameraTransform);
}

void UWorld::HandleKeyboardCameraMoveRequest(const FKeyboardCameraMoveRequestMessage& Message) {
    UCameraComponent* Camera{GetCameraSubsystem().GetMainCamera()};
    if (Camera == nullptr || Message.DeltaTime <= 0.0f) {
        return;
    }

    const FMatrix CameraWorldMatrix{Camera->GetComponentToWorld()};
    FVector3 MoveDirection{CameraWorldMatrix.Forward() * Message.ForwardAxis + CameraWorldMatrix.Right() * Message.RightAxis};
    if (MoveDirection.LengthSquared() <= 0.0f) {
        return;
    }

    MoveDirection.Normalize();
    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};

    Camera->SetRelativeLocation(Camera->GetRelativeLocation() + MoveDirection * Settings.mMoveSensitivity * Message.DeltaTime);
}

void UWorld::HandleMouseCameraMoveRequestMessage(const FMouseCameraMoveRequestMessage& Message) {
    UCameraComponent* Camera{GetCameraSubsystem().GetMainCamera()};
    if (Camera == nullptr) {
        return;
    }

    const FMatrix CameraWorld{Camera->GetComponentToWorld()};
    FVector3 Right{CameraWorld.Right()};
    FVector3 Up{CameraWorld.Up()};
    Right.Normalize();
    Up.Normalize();
    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};
    const float PanScale{Settings.mMoveSensitivity * 0.01f};
    const FVector3 Offset{Right * (-Message.DeltaX * PanScale) + Up * (-Message.DeltaY * PanScale)};

    Camera->SetRelativeLocation(Camera->GetRelativeLocation() + Offset);
}

void UWorld::HandleMouseCameraDollyRequestMessage(const FMouseCameraDollyRequestMessage& Message) {
    UCameraComponent* Camera{GetCameraSubsystem().GetMainCamera()};
    if (Camera == nullptr) {
        return;
    }

    FVector3 ForwardDirection{Camera->GetComponentToWorld().Forward()};
    ForwardDirection.Normalize();
    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};
    const float DollySpeed{Settings.mMoveSensitivity * 0.3f};

    Camera->SetRelativeLocation(Camera->GetRelativeLocation() + ForwardDirection * (Message.Steps * DollySpeed));
}
#endif

AActor* UWorld::AddActor(std::unique_ptr<AActor> InActor) {
    if (!InActor) {
        return nullptr;
    }

    AActor* Actor{InActor.get()};

    if (UObjectSystem::Resolve(Actor->GetHandle()) != Actor) {
        UObjectSystem::Register(Actor);
    }

    mActors.push_back(std::move(InActor));

    Actor->SetWorld(this);

    MarkOutlinerDirty();

    return Actor;
}

void UWorld::HandleSpawnComponent(const FMessageSpawnComponent& Message, const IAssetRegistry* AssetRegistry) {
    static std::mt19937 RandomEngine{std::random_device{}()};
    const FTypeInfo* ComponentType{TypeRegistry::Find(Message.mComponentType)};
    if (ComponentType == nullptr || ComponentType->mCreator == nullptr ||
        !ComponentType->IsA(UActorComponent::StaticTypeInfo())) {
        return;
    }

    const bool BIsStaticMesh{ComponentType->IsA(UStaticMeshComponent::StaticTypeInfo())};
    const FAssetHandle MeshHandle{BIsStaticMesh ? AssetRegistry->FindAsset(FAssetPath{Message.mMeshType}) : FAssetHandle{}};
    if (BIsStaticMesh && AssetRegistry->ResolveAsset<UMesh>(MeshHandle) == nullptr) {
        return;
    }
    const FAssetHandle PipelineHandle{BIsStaticMesh ? AssetRegistry->FindAsset(FAssetPath{"/Game/Pipeline/Base"}) : FAssetHandle{}};
    const FAssetHandle Materials[]{ AssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Default.mtl"}), AssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Red.mtl"}), AssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Green.mtl"}), AssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Blue.mtl"})};
    const bool IsBillboard{ComponentType->IsA(UBillboardComponent::StaticTypeInfo())};
    const bool IsLight{ComponentType->IsA(ULightComponent::StaticTypeInfo())};
    const FAssetHandle BillboardPipeline{IsBillboard || IsLight ? AssetRegistry->FindAsset(FAssetPath{"/Game/Pipeline/Billboard.json"}) : FAssetHandle{}};
    const FAssetHandle BillboardTexture{IsBillboard ? AssetRegistry->FindAsset(FAssetPath{"/Game/Texture/Fire+Sparks-Sheet.png"}) : FAssetHandle{}};
    const FAssetHandle LightProxyTexture{IsLight ? AssetRegistry->FindAsset(FAssetPath{"/Game/System/Light.png"}) : FAssetHandle{}};

    std::uniform_int_distribution<std::size_t> MaterialIndex{0, std::size(Materials) - 1};
    const FAssetHandle MaterialHandle{BIsStaticMesh ? Materials[MaterialIndex(RandomEngine)] : FAssetHandle{}};

    std::uniform_real_distribution<float> RandomX{-5.0f, 5.0f};
    std::uniform_real_distribution<float> RandomY{-5.0f, 5.0f};
    std::uniform_real_distribution<float> RandomZ{-3.0f, 3.0f};

    const FVector3 SpawnCenter{0.0f, 0.0f, 5.0f};

    for (Uint32 Index{0}; Index < Message.mSpawnCount; ++Index) {
        AActor* Actor{AdoptActor<AActor>()};
        if (Actor == nullptr) {
            continue;
        }

        UBillboardComponent* LightProxy{};
        if (IsLight) {
            LightProxy = Actor->AddComponent<UBillboardComponent>();
            if (LightProxy == nullptr || !Actor->SetRootComponent(LightProxy)) {
                DestroyActor(Actor);
                continue;
            }
            LightProxy->SetPipelineHandle(BillboardPipeline);
            LightProxy->SetTextureHandle(LightProxyTexture);
        }

        UActorComponent* Component{Actor->AddComponent(*ComponentType)};
        if (Component == nullptr) {
            DestroyActor(Actor);
            continue;
        }

        if (ComponentType->IsA(USceneComponent::StaticTypeInfo())) {
            USceneComponent* SceneComponent{static_cast<USceneComponent*>(Component)};
            if (IsLight) {
                if (!SceneComponent->AttachToComponent(LightProxy)) {
                    DestroyActor(Actor);
                    continue;
                }
            } else {
                Actor->SetRootComponent(SceneComponent);
            }
            USceneComponent* SpawnRoot{IsLight ? LightProxy : SceneComponent};
            SpawnRoot->SetRelativeLocation(FVector3{ SpawnCenter.mX + RandomX(RandomEngine), SpawnCenter.mY + RandomY(RandomEngine), SpawnCenter.mZ + RandomZ(RandomEngine)});
        }

        if (BIsStaticMesh) {
            auto* StaticMeshComponent{static_cast<UStaticMeshComponent*>(Component)};
            StaticMeshComponent->SetMeshHandle(MeshHandle);
            StaticMeshComponent->SetPipelineHandle(PipelineHandle);
            StaticMeshComponent->SetMaterialHandle(MaterialHandle);
        }
        if (IsBillboard) {
            auto* Billboard{static_cast<UBillboardComponent*>(Component)};
            Billboard->SetPipelineHandle(BillboardPipeline);
            Billboard->SetTextureHandle(BillboardTexture);
        }

        auto Tag{Actor->AddComponent<UNameTagComponent>()};
        Tag->SetActive(false);
    }

    FlushPendingDestroyActors();
}

const IAssetRegistry* UWorld::GetAssetRegistry() const {
    return mAssetRegistry;
}

void UWorld::ClearActors() {
    for (auto& CurrentActor : mActors) {
        DestroyActor(CurrentActor.get());
    }
    FlushPendingDestroyActors();
}

FName UWorld::MakeUniqueObjectName(std::string_view SourceName) {
    std::string_view BaseName{};
    Int32 Number{0};

    SplitNameAndNumber(SourceName, BaseName, Number);

    Int32 Index{(Number > 0) ? (Number + 1) : 1};

    if ((Number == 0) && (FindActorByName(BaseName) == nullptr)) {
        return FName{BaseName};
    }

    while (true) {
        FName CandidateName{BaseName, Index};

        if (FindActorByName(CandidateName) == nullptr) {
            return CandidateName;
        }

        Index++;
    }

    return FName{};
}

AActor* UWorld::FindActorByName(FName InName) const {
    for (const auto& Actor : mActors) {
        if (Actor && Actor->GetName() == InName) {
            return Actor.get();
        }
    }

    return nullptr;
}

void UWorld::SetAssetRegistry(const IAssetRegistry* InAssetRegistry, IAssetRegistryMutator* InAssetRegistryMutator) {
    mAssetRegistry = InAssetRegistry;
    mAssetRegistryMutator = InAssetRegistryMutator;
}

IAssetRegistryMutator* UWorld::GetAssetRegistryMutator() const {
    return mAssetRegistryMutator;
}
