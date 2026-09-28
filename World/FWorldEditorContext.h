#pragma once

#include "Core/Base/FAssetHandle.h"
#include "Core/Base/TObjectRef.h"
#include "Core/Channel/FMessageChannel.h"
#include "Core/Channel/FStateChannel.h"
#include "Core/Channel/FEditorInfo.h"
#include "Core/Channel/FEditorSettings.h"
#include "World/AActor.h"
#include "Core/Asset/IAssetRegistry.h"
#include "World/Component/UActorComponent.h"
#include "World/Component/USceneComponent.h"

class UWorld;

struct FWorldEditorSharedState {
    FEditorSettings mEditorSettings{};
    std::size_t mModeIndex{0};
};

class FWorldEditorContext {
public:
    void SetWorld(UWorld* InWorld);
    void InitializeChannels(const IAssetRegistry* AssetRegistry);
    void Dispatch();

    FMessageChannel::FSender GetEditorToWorldSender();
    FMessageChannel::FSender GetWorldToEditorSender();

    FEditorSettings GetEditorSettings() const;
    void SetEditorSettings(const FEditorSettings& Settings);
    void SetMoveSensitivity(float Value);
    void SetRotationSensitivity(float Value);
    void SetGridSize(float Value);
    void SetGridSnapEnabled(bool Enabled);
    void SetGridVisible(bool Visible);
    void SetAxisVisible(bool Visible);

    const std::size_t GetRenderModeState() const noexcept;
    void SetRenderModeState(const std::size_t State);

    void SetSelectedActor(AActor* Actor);
    void SetSelectedComponent(UActorComponent* Component);
    void ClearSelection();

    AActor* GetSelectedActor() const noexcept;
    UActorComponent* GetSelectedComponent() const noexcept;
    USceneComponent* GetSelectedTransformTarget() const noexcept;

    UWorld* GetWorld() const;


    // 프리뷰 대상을 바꾸면 Viewer 창을 띄워달라는 요청도 같이 세운다.
    void SetPreviewMesh(const FAssetHandle& Handle);
    FAssetHandle GetPreviewMesh() const noexcept;
    FAssetHandle ConsumePreviewMesh() noexcept;

    // 요청을 한 번만 처리하도록 읽으면서 내린다.
    bool ConsumePreviewOpenRequest() noexcept;

private:
    UWorld* mWorld{nullptr};
    TObjectRef<AActor> mSelectedActor{};
    TObjectRef<UActorComponent> mSelectedComponent{};
    FStateChannel<FWorldEditorSharedState> mSharedState{std::in_place};
    FMessageChannel mEditorToWorld{64};
    FMessageChannel mWorldToEditor{64};

    FAssetHandle mPreviewMesh{};
    bool mBPreviewOpenRequested{false};
};
