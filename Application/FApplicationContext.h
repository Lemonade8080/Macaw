#pragma once

#include <memory>

#include "Asset/FAssetRegistry.h"
#include "Core/Channel/FMessageChannel.h"
#include "Editor/Input/FKeyboardInput.h"
#include "Editor/Input/FMouseInput.h"
#include "Editor/Panel/FEditorUIManager.h"
#include "Editor/Panel/IEditorPanel.h"
#include "Editor/Settings/FEditorConfigManager.h"
#include "Editor/View/EditorViewport.h"
#include "Editor/View/FAssetThumbnailRenderer.h"
#include "Render/Renderer.h"
#include "World/FWorldEditorContext.h"
#include "World/UWorld.h"

struct FApplicationContext {
    FRenderer mRenderer{};
    FMouseInput mMouseInput{};
    FKeyboardInput mKeyboardInput{};
    std::unique_ptr<UWorld> mWorld{};
    std::unique_ptr<FWorldEditorContext> mEditorContext{};
    std::unique_ptr<FAssetRegistry> mAssetRegistry{};
    std::unique_ptr<FAssetThumbnailRenderer> mThumbnailRenderer{};
    std::unique_ptr<FMessageChannel> mWorldCommandChannel{};
    std::unique_ptr<EditorViewport> mEditorView{};
    std::unique_ptr<FEditorUIManager> mEditorUIManager{};
    std::unique_ptr<IEditorPanel> mMenuPanel{};
    FEditorSettings mEditorSettings{};
};
