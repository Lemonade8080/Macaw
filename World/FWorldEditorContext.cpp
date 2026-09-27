#include "pch.h"
#include "FWorldEditorContext.h"

#include "AActor.h"
#include "Component/UActorComponent.h"
#include "Component/USceneComponent.h"
#include "Core/Asset/IAssetRegistry.h"
#include "UWorld.h"

#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

namespace {
    bool ReWriteObjFilePath(const std::filesystem::path& MetaPath, const FString& NewObjFilePath) {
        std::ifstream InputStream{MetaPath, std::ios::binary};
        if (!InputStream.is_open()) {
            return false;
        }

        rapidjson::IStreamWrapper InStreamWrapper{InputStream};

        rapidjson::Document Document{};
        Document.ParseStream<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(InStreamWrapper);

        //읽기 닫기
        InputStream.close();

        if (Document.HasParseError() || !Document.IsObject())
            return false;

        rapidjson::Document::AllocatorType& Allocator{Document.GetAllocator()};

        if (Document.HasMember("FilePath")) {
            Document["FilePath"].SetString(NewObjFilePath.c_str(), Allocator);
        } else {
            Document.AddMember("FilePath", rapidjson::Value(NewObjFilePath.c_str(), Allocator), Allocator);
        }

        std::ofstream OutputStream{MetaPath};
        if (!OutputStream.is_open())
            return false;

        rapidjson::OStreamWrapper OutStreamWrapper{OutputStream};
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> Writer{OutStreamWrapper};
        Document.Accept(Writer);

        return true;
    }
}

void FWorldEditorContext::SetWorld(UWorld* InWorld) {
    mWorld = InWorld;
}

void FWorldEditorContext::InitializeChannels(const IAssetRegistry* AssetRegistry) {
    if (mWorld == nullptr)
        return;

    mEditorToWorld.TryBind<FMessageSpawnComponent>([this, AssetRegistry](const FMessageSpawnComponent& Message) {
        mWorld->HandleSpawnComponent(Message, AssetRegistry);
    });
    mEditorToWorld.TryBind<FMessageSaveScene>([this, AssetRegistry](const FMessageSaveScene& Message) {
        mWorld->SaveScene(Message.mSceneName, AssetRegistry);
    });
    mEditorToWorld.TryBind<FMessageLoadScene>([this](const FMessageLoadScene& Message) {
        mWorld->LoadScene(std::filesystem::path(Message.mFilePath.c_str()));
    });
}

void FWorldEditorContext::Dispatch() {
    mEditorToWorld.Dispatch();
    mWorldToEditor.Dispatch();
}

FMessageChannel::FSender FWorldEditorContext::GetEditorToWorldSender() {
    return mEditorToWorld.GetSender();
}

FMessageChannel::FSender FWorldEditorContext::GetWorldToEditorSender() {
    return mWorldToEditor.GetSender();
}

FEditorSettings FWorldEditorContext::GetEditorSettings() const {
    return mSharedState.GetReader().Peek().mEditorSettings;
}

void FWorldEditorContext::SetEditorSettings(const FEditorSettings& Settings) {
    mSharedState.GetWriter().Modify([&Settings](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings = Settings;
    });
}

void FWorldEditorContext::SetMoveSensitivity(float Value) {
    mSharedState.GetWriter().Modify([Value](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings.mMoveSensitivity = Value;
    });
}

void FWorldEditorContext::SetRotationSensitivity(float Value) {
    mSharedState.GetWriter().Modify([Value](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings.mRotationSensitivity = Value;
    });
}

void FWorldEditorContext::SetGridSize(float Value) {
    mSharedState.GetWriter().Modify([Value](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings.mGridSize = Value;
    });
}

void FWorldEditorContext::SetGridSnapEnabled(bool Enabled) {
    mSharedState.GetWriter().Modify([Enabled](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings.mGridSnapEnabled = Enabled;
    });
}

void FWorldEditorContext::SetGridVisible(bool Visible) {
    mSharedState.GetWriter().Modify([Visible](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings.mGridVisible = Visible;
    });
}

void FWorldEditorContext::SetAxisVisible(bool Visible) {
    mSharedState.GetWriter().Modify([Visible](FWorldEditorSharedState& Shared) {
        Shared.mEditorSettings.mAxisVisible = Visible;
    });
}

const std::size_t FWorldEditorContext::GetRenderModeState() const noexcept {
    return mSharedState.GetReader().Peek().mModeIndex;
}

void FWorldEditorContext::SetRenderModeState(const std::size_t State) {
    mSharedState.GetWriter().Modify([&State](FWorldEditorSharedState& Shared) {
        Shared.mModeIndex = State;
    });
}

void FWorldEditorContext::SetSelectedActor(AActor* Actor) {
    if (Actor == nullptr) {
        ClearSelection();
        return;
    }

    mSelectedActor.Set(Actor);
    mSelectedComponent.Set(Actor->GetRootComponent());
}

void FWorldEditorContext::SetSelectedComponent(UActorComponent* Component) {
    if (Component == nullptr || Component->GetOwner() == nullptr) {
        ClearSelection();
        return;
    }

    mSelectedActor.Set(Component->GetOwner());
    mSelectedComponent.Set(Component);
}

void FWorldEditorContext::ClearSelection() {
    mSelectedComponent.Reset();
    mSelectedActor.Reset();
}

AActor* FWorldEditorContext::GetSelectedActor() const noexcept {
    return mSelectedActor.Get();
}

UActorComponent* FWorldEditorContext::GetSelectedComponent() const noexcept {
    return mSelectedComponent.Get();
}

USceneComponent* FWorldEditorContext::GetSelectedTransformTarget() const noexcept {
    UActorComponent* Component{mSelectedComponent.Get()};
    if (Component != nullptr && Component->GetTypeInfo()->IsA(USceneComponent::StaticTypeInfo())) {
        return static_cast<USceneComponent*>(Component);
    }

    AActor* Actor{mSelectedActor.Get()};
    return Actor != nullptr ? Actor->GetRootComponent() : nullptr;
}

UWorld* FWorldEditorContext::GetWorld() const {
    return mWorld;
}

void FWorldEditorContext::SetPreviewMesh(const FAssetHandle& Handle) {
    mPreviewMesh = Handle;
    mBPreviewOpenRequested = true;
}

FAssetHandle FWorldEditorContext::GetPreviewMesh() const noexcept {
    return mPreviewMesh;
}

FAssetHandle FWorldEditorContext::ConsumePreviewMesh() noexcept {
    const FAssetHandle Handle{mPreviewMesh};
    mPreviewMesh = {};
    return Handle;
}

bool FWorldEditorContext::ConsumePreviewOpenRequest() noexcept {
    const bool Requested{mBPreviewOpenRequested};
    mBPreviewOpenRequested = false;
    return Requested;
}
