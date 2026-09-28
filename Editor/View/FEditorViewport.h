#pragma once

#include "FEditorViewportTypes.h"
#include "FViewportGeometry.h"
#include "Core/Base/FRenderProbe.h"
#include "Render/FSceneRenderSurface.h"
#include "Editor/View/EditorViewport.h"
#include "Editor/Input/FKeyboardInput.h"
#include "Editor/Input/FMouseInput.h"
#include "World/FWorldEditorContext.h"
#include "ImGui/imgui.h"

class FStatPanel;

class FEditorViewport {
public:
    FEditorViewport(FViewportId InViewportId, ID3D11Device* InDevice, FWorldEditorContext& InEditorContext);

    FEditorViewport(const FEditorViewport&) = delete;
    FEditorViewport& operator=(const FEditorViewport&) = delete;
    FEditorViewport(FEditorViewport&&) = delete;
    FEditorViewport& operator=(FEditorViewport&&) = delete;

    FViewportId GetViewportId() const;

    void BeginFrame();
    bool Draw(const FRect& Rect, const ImVec2& MainViewportPosition, bool BInputBlocked, FStatPanel* StatOverlay = nullptr);
    void SetFocused(bool BInFocused);
    void ProcessInput(EditorViewport& SharedEditorViewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime, bool BInputBlocked);
    bool PrepareForRender();
    bool BuildCameraProbe(CameraProbe& OutCamera);

    FSceneRenderSurface& GetRenderSurface();
    const D3D11_VIEWPORT& GetRenderViewport() const;
    const FRenderSettings& GetRenderSettings() const;
    const FVector3& GetCameraPosition() const;

    void ReleaseRenderResources();

    void SetProjectionType(EProjectionType InProjectionType);
    void SetOrthographicView(EOrthographicView InView);
    void SetCameraParameter(const FVector3& InPosition, const FQuat& InRotation, float InFieldOfView, float InOrthographicWidth, float InNearPlane, float InFarPlane);

private:
    bool DrawMenuBar();
    bool SpawnDroppedStaticMesh(FAssetHandle MeshHandle, const ImVec2& ScreenPosition);
    bool TryCalculateDropPosition(const ImVec2& ScreenPosition, FVector3& OutPosition);
    void ApplyOrthographicView();
    const char* GetViewModeName() const;
    void ResizeRenderSurface();
    void ApplyMouseNavigation(const FViewportMouseNavigationInput& NavigationInput);
    void ApplyKeyboardNavigation(const FViewportKeyboardNavigationInput& NavigationInput);
    D3D11_VIEWPORT BuildInputViewport() const;

    ID3D11Device* mDevice{nullptr};
    FWorldEditorContext* mEditorContext{nullptr};
    FSceneRenderSurface mRenderSurface{};
    FRenderSettings mRenderSettings{};
    FViewportId mViewportId{0};

    EProjectionType mProjectionType{EProjectionType::Perspective};
    EOrthographicView mOrthographicView{EOrthographicView::Front};

    FVector3 mCameraPosition{-13.567042f, -26.165287f, 31.506821f};
    FQuat mCameraRotation{-0.040121f, -0.205543f, -0.187332f, 0.959713f};
    FVector3 mPerspectiveCameraPosition{mCameraPosition};
    FQuat mPerspectiveCameraRotation{mCameraRotation};
    FVector3 mOrthographicTarget{};

    float mFieldOfView{1.0472f};
    float mOrthographicWidth{50.0f};
    float mNearPlane{0.1f};
    float mFarPlane{1000.0f};

    FRect mDisplayRect{};
    Uint32 mWidth{0};
    Uint32 mHeight{0};
    float mRenderLeft{0.0f};
    float mRenderTop{0.0f};
    bool mBVisible{false};
    bool mBHovered{false};
    bool mBFocused{false};
};
