#include "pch.h"
#include "FViewApplication.h"

#include "Core/Channel/Messages/FMouseCameraRotateRequestMessage.h"
#include "Core/Channel/Messages/FKeyboardCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraDollyRequestMessage.h"
#include "Editor/Panel/FViewerToolBar.h"

FViewApplication::FViewApplication() = default;

FViewApplication::~FViewApplication() = default;

void FViewApplication::InitializeMode(FApplicationContext& Context, HWND WindowHandle) {
    Context.mWorldCommandChannel->TryBind<FMouseCameraRotateRequestMessage>([&Context](const FMouseCameraRotateRequestMessage& Message) {
        Context.mWorld->HandleMouseCameraRotateRequest(Message);
    });
    Context.mWorldCommandChannel->TryBind<FKeyboardCameraMoveRequestMessage>([&Context](const FKeyboardCameraMoveRequestMessage& Message) {
        Context.mWorld->HandleKeyboardCameraMoveRequest(Message);
    });
    Context.mWorldCommandChannel->TryBind<FMouseCameraMoveRequestMessage>([&Context](const FMouseCameraMoveRequestMessage& Message) {
        Context.mWorld->HandleMouseCameraMoveRequestMessage(Message);
    });
    Context.mWorldCommandChannel->TryBind<FMouseCameraDollyRequestMessage>([&Context](const FMouseCameraDollyRequestMessage& Message) {
        Context.mWorld->HandleMouseCameraDollyRequestMessage(Message);
    });

    Context.mMenuPanel = std::make_unique<FViewerToolBar>(*Context.mEditorContext);
    Context.mEditorUIManager->InitializeViewer(*Context.mAssetRegistry, WindowHandle, *Context.mEditorContext, Context.mThumbnailRenderer.get());
}

void FViewApplication::TickMode(FApplicationContext&, float) {
}
