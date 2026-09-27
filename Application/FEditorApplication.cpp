#include "pch.h"
#include "FEditorApplication.h"
#include "Core/Stat/Stat.h"

#include "Editor/Panel/FControlPanel.h"
#include "Editor/View/FEditorViewport.h"

FEditorApplication::FEditorApplication() = default;

FEditorApplication::~FEditorApplication() = default;

void FEditorApplication::InitializeMode(FApplicationContext& Context, HWND WindowHandle) {
    if (Context.mEditorContext->GetEditorSettings().mControlPanelEnabled) {
        Context.mMenuPanel = std::make_unique<FControlPanel>(*Context.mEditorContext, WindowHandle, Context.mEditorContext->GetEditorToWorldSender());
    }
    Context.mEditorUIManager->Initialize(*Context.mWorld, Context.mRenderer, *Context.mAssetRegistry, *Context.mEditorContext, WindowHandle, Context.mEditorView->GetGizmoMode(), Context.mEditorView->GetGizmoCoordinateSpace(), Context.mThumbnailRenderer.get());
}

void FEditorApplication::TickMode(FApplicationContext& Context, float DeltaTime) {
    FViewportHostWindow* ViewportHostWindow{Context.mEditorUIManager->GetViewportHostWindow()};
    if (ViewportHostWindow != nullptr) {
        const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::Input};
        ViewportHostWindow->ProcessInput(*Context.mEditorView, Context.mKeyboardInput, Context.mMouseInput, DeltaTime);
    }
    {
        const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::WorldCommands};
        Context.mWorldCommandChannel->Dispatch();
    }
    {
        const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::WorldTick};
        Context.mWorld->Tick(DeltaTime);
    }
    {
        const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::EditorDispatch};
        Context.mEditorContext->Dispatch();
    }

    if (ViewportHostWindow == nullptr) {
        return;
    }

    FSceneRenderData Scene{};
    Context.mWorld->BuildSceneRenderData(Scene);
    const AActor* SelectedActor{Context.mEditorContext->GetSelectedActor()};
    const FObjectHandle SelectedActorHandle{SelectedActor != nullptr ? SelectedActor->GetHandle() : FObjectHandle{}};

    for (FViewportId Id{}; Id < FViewportHostWindow::MaximumViewportCount; ++Id) {
        FEditorViewport* Viewport{ViewportHostWindow->PrepareViewportForRender(Id)};
        if (Viewport == nullptr) {
            continue;
        }

        const Stat::FScopedSystemStatTimer StageStat{Stat::ESystemStatStage::SceneRender};
        CameraProbe Camera{};
        if (!Viewport->BuildCameraProbe(Camera)) {
            continue;
        }

        FRenderView View{};
        View.mTarget = &Viewport->GetRenderSurface();
        View.mCamera = Camera;
        View.mSettings = Viewport->GetRenderSettings();
        View.mRenderMode = static_cast<ERenderMode>(Context.mEditorContext->GetRenderModeState());
        View.mSelectedActorHandle = SelectedActorHandle;
        Context.mEditorView->BuildViewRenderData(View, Camera, Viewport->GetCameraPosition(), Viewport->GetRenderViewport());
        Context.mRenderer.RenderView(View, Scene);
    }
}
