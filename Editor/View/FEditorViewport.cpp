#include "pch.h"

#include <algorithm>
#include <cmath>

#include "FEditorViewport.h"
#include "Editor/Panel/FStatPanel.h"

#include "EditorViewport.h"
#include "Editor/Input/FKeyboardInput.h"
#include "Editor/Input/FMouseInput.h"
#include "ImGui/imgui.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMesh.h"
#include "Core/Base/FTransform.h"
#include "World/AActor.h"
#include "World/Component/UCameraComponent.h"
#include "World/FWorldEditorContext.h"
#include "World/Subsystem/UPickingSubsystem.h"
#include "World/UWorld.h"

#include "../../Core/Console/Console.h"

namespace {
    constexpr float HalfSqrtTwo{0.70710678118f};
    constexpr float DefaultDropDistance{10.0f};
    constexpr float DropPlaneEpsilon{0.000001f};
    constexpr char StaticMeshAssetPayloadType[]{"MACAW_STATIC_MESH_ASSET"};

    const char* GetOrthographicViewName(EOrthographicView View) {
        switch (View) {
            case EOrthographicView::Front:
                return "Front";
            case EOrthographicView::Back:
                return "Back";
            case EOrthographicView::Left:
                return "Left";
            case EOrthographicView::Right:
                return "Right";
            case EOrthographicView::Top:
                return "Top";
            case EOrthographicView::Bottom:
                return "Bottom";
            default:
                return "Orthographic";
        }
    }

    FQuat GetOrthographicRotation(EOrthographicView View) {
        switch (View) {
            case EOrthographicView::Front:
                return {0.0f, 0.0f, 1.0f, 0.0f};
            case EOrthographicView::Back:
                return {};
            case EOrthographicView::Left:
                return {0.0f, 0.0f, -HalfSqrtTwo, HalfSqrtTwo};
            case EOrthographicView::Right:
                return {0.0f, 0.0f, HalfSqrtTwo, HalfSqrtTwo};
            case EOrthographicView::Top:
                return {0.0f, HalfSqrtTwo, 0.0f, HalfSqrtTwo};
            case EOrthographicView::Bottom:
                return {0.0f, -HalfSqrtTwo, 0.0f, HalfSqrtTwo};
            default:
                return {};
        }
    }
}

FEditorViewport::FEditorViewport(FViewportId InViewportId, ID3D11Device* InDevice, FWorldEditorContext& InEditorContext)
    : mDevice(InDevice),
      mEditorContext(&InEditorContext),
      mViewportId(InViewportId),
      mProjectionType(InViewportId == 0 ? EProjectionType::Perspective : EProjectionType::Orthographic) {
    mCameraRotation.Normalize();
    mRenderSettings.mBRenderSky = mProjectionType == EProjectionType::Perspective;

    if (mProjectionType == EProjectionType::Orthographic) {
        const EOrthographicView InitialViews[]{ EOrthographicView::Front, EOrthographicView::Top, EOrthographicView::Front, EOrthographicView::Right};
        mOrthographicView = InitialViews[std::min<std::size_t>(mViewportId, std::size(InitialViews) - 1)];
        ApplyOrthographicView();
    }

    if (mDevice != nullptr) {
        mRenderSurface.InitializeOffscreen(mDevice, 1, 1);
    }
}

FViewportId FEditorViewport::GetViewportId() const {
    return mViewportId;
}

void FEditorViewport::BeginFrame() {
    mDisplayRect = {};
    mWidth = 0;
    mHeight = 0;
    mRenderLeft = 0.0f;
    mRenderTop = 0.0f;
    mBVisible = false;
    mBHovered = false;
    mBFocused = false;
}

bool FEditorViewport::Draw(const FRect& Rect, const ImVec2& MainViewportPosition, bool BInputBlocked, FStatPanel* StatOverlay) {
    if (Rect.IsEmpty()) {
        return false;
    }

    mDisplayRect = Rect;

    const ImVec2 Position{static_cast<float>(Rect.mMin.mX), static_cast<float>(Rect.mMin.mY)};
    const ImVec2 Size{static_cast<float>(Rect.GetWidth()), static_cast<float>(Rect.GetHeight())};
    ImGui::SetCursorScreenPos(Position);
    ImGui::PushID(static_cast<int>(mViewportId));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool BChildVisible{ImGui::BeginChild("##SceneViewport", Size, ImGuiChildFlags_None, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)};
    bool BActivated{false};

    if (BChildVisible) {
        BActivated = DrawMenuBar();

        const ImVec2 ImagePosition{ImGui::GetCursorScreenPos()};
        const ImVec2 ImageSize{ImGui::GetContentRegionAvail()};

        if (ImageSize.x > 0.0f && ImageSize.y > 0.0f) {
            mWidth = static_cast<Uint32>(ImageSize.x);
            mHeight = static_cast<Uint32>(ImageSize.y);
            mRenderLeft = ImagePosition.x - MainViewportPosition.x;
            mRenderTop = ImagePosition.y - MainViewportPosition.y;
            mDisplayRect = { {static_cast<Int32>(ImagePosition.x), static_cast<Int32>(ImagePosition.y)}, {static_cast<Int32>(ImagePosition.x + ImageSize.x), static_cast<Int32>(ImagePosition.y + ImageSize.y)}};
            mBVisible = true;
            ResizeRenderSurface();

            ImGui::InvisibleButton("##SceneSurface", ImageSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

            if (ID3D11ShaderResourceView * ShaderResourceView{mRenderSurface.GetShaderResourceView()}) {
                ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(ShaderResourceView), ImagePosition, ImVec2(ImagePosition.x + ImageSize.x, ImagePosition.y + ImageSize.y));
            }

            if (ImGui::BeginDragDropTarget()) {
                const ImGuiPayload* Payload{ImGui::AcceptDragDropPayload(StaticMeshAssetPayloadType)};
                if (Payload != nullptr && Payload->IsDelivery() && Payload->DataSize == sizeof(FAssetHandle)) {
                    const FAssetHandle MeshHandle{*static_cast<const FAssetHandle*>(Payload->Data)};
                    BActivated = SpawnDroppedStaticMesh(MeshHandle, ImGui::GetMousePos()) || BActivated;
                }
                ImGui::EndDragDropTarget();
            }

            if (StatOverlay != nullptr) {
                StatOverlay->Draw(ImagePosition, ImVec2{ImagePosition.x + ImageSize.x, ImagePosition.y + ImageSize.y});
            }

            mBHovered = !BInputBlocked && ImGui::IsItemHovered();
            BActivated = BActivated || (mBHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopID();
    return BActivated;
}

bool FEditorViewport::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return false;
    }

    bool BActivated{false};
    ImGui::TextUnformatted("View");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(120.0f, ImGui::GetContentRegionAvail().x));
    const bool BComboOpen{ImGui::BeginCombo("##ViewMode", GetViewModeName())};
    BActivated = ImGui::IsItemActivated();
    if (BComboOpen) {
        const bool BPerspectiveSelected{mProjectionType == EProjectionType::Perspective};
        if (ImGui::Selectable("Perspective", BPerspectiveSelected)) {
            SetProjectionType(EProjectionType::Perspective);
            BActivated = true;
        }
        if (BPerspectiveSelected) {
            ImGui::SetItemDefaultFocus();
        }

        constexpr EOrthographicView Views[]{ EOrthographicView::Front, EOrthographicView::Back, EOrthographicView::Left, EOrthographicView::Right, EOrthographicView::Top, EOrthographicView::Bottom};
        for (EOrthographicView View : Views) {
            const bool BSelected{mProjectionType == EProjectionType::Orthographic && mOrthographicView == View};
            if (ImGui::Selectable(GetOrthographicViewName(View), BSelected)) {
                SetOrthographicView(View);
                BActivated = true;
            }
            if (BSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::EndMenuBar();
    return BActivated;
}

bool FEditorViewport::SpawnDroppedStaticMesh(FAssetHandle MeshHandle, const ImVec2& ScreenPosition) {
    UWorld* World{mEditorContext != nullptr ? mEditorContext->GetWorld() : nullptr};
    const IAssetRegistry* AssetRegistry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    if (World == nullptr || AssetRegistry == nullptr || AssetRegistry->ResolveAsset<UMesh>(MeshHandle) == nullptr) {
        return false;
    }

    FVector3 DropPosition{};
    if (!TryCalculateDropPosition(ScreenPosition, DropPosition)) {
        return false;
    }

    const FAssetHandle PipelineHandle{AssetRegistry->EnsureDefaultStaticMeshPipeline()};
    const FAssetHandle MaterialHandle{AssetRegistry->EnsureDefaultStaticMeshMaterial()};
    AActor* Actor{World->SpawnActor(MeshHandle, PipelineHandle, MaterialHandle, DropPosition)};
    if (Actor == nullptr) {
        return false;
    }

    mEditorContext->SetSelectedActor(Actor);
    return true;
}

bool FEditorViewport::TryCalculateDropPosition(const ImVec2& ScreenPosition, FVector3& OutPosition) {
    if (!mBVisible || mWidth == 0 || mHeight == 0) {
        return false;
    }

    CameraProbe Camera{};
    if (!BuildCameraProbe(Camera)) {
        return false;
    }

    const float NdcX{2.0f * (ScreenPosition.x - static_cast<float>(mDisplayRect.mMin.mX)) / static_cast<float>(mWidth) - 1.0f};
    const float NdcY{1.0f - 2.0f * (ScreenPosition.y - static_cast<float>(mDisplayRect.mMin.mY)) / static_cast<float>(mHeight)};
    FMatrix InverseViewProjection{};
    if (!Camera.mViewProjection.TryInverse(InverseViewProjection)) {
        return false;
    }

    FVector3 RayOrigin{};
    FVector3 RayEnd{};
    if (!InverseViewProjection.TransformCoord({NdcX, NdcY, 0.0f}, RayOrigin) || !InverseViewProjection.TransformCoord({NdcX, NdcY, 1.0f}, RayEnd)) {
        return false;
    }

    FVector3 RayDirection{RayEnd - RayOrigin};
    if (RayDirection.LengthSquared() <= DropPlaneEpsilon) {
        return false;
    }
    RayDirection.Normalize();

    UWorld* World{mEditorContext != nullptr ? mEditorContext->GetWorld() : nullptr};
    UPrimitiveComponent* HitComponent{};
    float HitDistance{};
    FMatrix CameraWorld{};
    if (World != nullptr && Camera.mView.TryInverse(CameraWorld) && World->GetPickingSubsystem().Raycast(FRay{RayOrigin.ToSimpleMath(), RayDirection.ToSimpleMath()}, HitComponent, HitDistance, &CameraWorld)) {
        OutPosition = RayOrigin + RayDirection * HitDistance;
        return true;
    }

    if (mProjectionType == EProjectionType::Orthographic) {
        const float TargetDistance{(mOrthographicTarget - RayOrigin).Dot(RayDirection)};
        if (TargetDistance >= 0.0f) {
            OutPosition = RayOrigin + RayDirection * TargetDistance;
            return true;
        }
    }

    if (std::abs(RayDirection.mZ) > DropPlaneEpsilon) {
        const float GroundDistance{-RayOrigin.mZ / RayDirection.mZ};
        if (GroundDistance >= 0.0f && GroundDistance <= mFarPlane) {
            OutPosition = RayOrigin + RayDirection * GroundDistance;
            return true;
        }
    }

    OutPosition = RayOrigin + RayDirection * DefaultDropDistance;
    return true;
}

void FEditorViewport::SetFocused(bool BInFocused) {
    mBFocused = BInFocused;
}

void FEditorViewport::ProcessInput(EditorViewport& SharedEditorViewport, FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, float DeltaTime, bool BInputBlocked) {
    if (mBVisible) {
        ResizeRenderSurface();
    }

    CameraProbe Camera{};
    const bool BHasCamera{BuildCameraProbe(Camera)};
    const bool BBlockMouse{BInputBlocked || !mBVisible || !mBHovered || !BHasCamera};

    if (BHasCamera) {
        SharedEditorViewport.PrepareInput(Camera, BuildInputViewport());
    }

    SharedEditorViewport.ProcessInput(KeyboardInput, MouseInput, BBlockMouse);
    const FViewportMouseNavigationInput MouseNavigation{MouseInput.DispatchPendingViewportCommands(static_cast<Int32>(mRenderLeft), static_cast<Int32>(mRenderTop), mWidth, mHeight, Camera.mViewProjection, Camera.mView, BBlockMouse)};

    if (BHasCamera) {
        ApplyMouseNavigation(MouseNavigation);
    }

    const bool BViewportNavigationActive{MouseInput.IsWorldDragActive(Right)};
    const bool BImGuiOwnsKeyboard{ImGui::GetIO().WantCaptureKeyboard && !BViewportNavigationActive};
    const bool BBlockKeyboard{mProjectionType == EProjectionType::Orthographic || BInputBlocked || !mBVisible || !mBFocused || !BHasCamera || BImGuiOwnsKeyboard};
    const FViewportKeyboardNavigationInput KeyboardNavigation{KeyboardInput.ConsumeViewportNavigation(DeltaTime, BBlockKeyboard)};

    if (BHasCamera) {
        ApplyKeyboardNavigation(KeyboardNavigation);
    }
}

bool FEditorViewport::PrepareForRender() {
    if (!mBVisible) {
        return false;
    }

    ResizeRenderSurface();
    return mRenderSurface.IsValid();
}

void FEditorViewport::ResizeRenderSurface() {
    if (mDevice == nullptr || mWidth == 0 || mHeight == 0) {
        return;
    }

    const D3D11_VIEWPORT& Viewport{mRenderSurface.GetViewport()};
    if (!mRenderSurface.IsValid() || Viewport.Width != static_cast<float>(mWidth) || Viewport.Height != static_cast<float>(mHeight)) {
        mRenderSurface.Resize(mDevice, mWidth, mHeight);
    }
}

bool FEditorViewport::BuildCameraProbe(CameraProbe& OutCamera) {
    if (mWidth == 0 || mHeight == 0) {
        return false;
    }

    const FTransform CameraTransform{mCameraPosition, mCameraRotation, FVector3{1.0f, 1.0f, 1.0f}};
    const float AspectRatio{static_cast<float>(mWidth) / static_cast<float>(mHeight)};
    OutCamera.mView = (UCameraComponent::CameraBasis * CameraTransform.ToMatrixNoScale()).Invert();

    if (mProjectionType == EProjectionType::Perspective) {
        OutCamera.mProjection = FMatrix::CreatePerspectiveFieldOfView(mFieldOfView, AspectRatio, mNearPlane, mFarPlane);
    } else {
        OutCamera.mProjection = FMatrix::CreateOrthographic(mOrthographicWidth, mOrthographicWidth / AspectRatio, mNearPlane, mFarPlane);
    }

    OutCamera.mViewProjection = OutCamera.mView * OutCamera.mProjection;

    FFrustum LocalFrustum{};
    FFrustum::CreateFromMatrix(LocalFrustum, OutCamera.mProjection.ToSimpleMath());
    LocalFrustum.Transform(OutCamera.mViewFrustum, OutCamera.mView.Inverse().ToSimpleMath());

    return true;
}

void FEditorViewport::ApplyMouseNavigation(const FViewportMouseNavigationInput& NavigationInput) {
    if (mProjectionType == EProjectionType::Orthographic) {
        if (NavigationInput.mWheelSteps != 0.0f) {
            mOrthographicWidth *= std::pow(0.85f, NavigationInput.mWheelSteps);
            mOrthographicWidth = std::clamp(mOrthographicWidth, 0.1f, 10000.0f);
        }

        if ((NavigationInput.mDragDeltaX != 0.0f || NavigationInput.mDragDeltaY != 0.0f) && mWidth > 0) {
            const FTransform CameraTransform{mCameraPosition, mCameraRotation, FVector3{1.0f, 1.0f, 1.0f}};
            const FMatrix CameraMatrix{CameraTransform.ToMatrixNoScale()};
            FVector3 CameraForward{CameraMatrix.Forward()};
            FVector3 PlaneUp{CameraMatrix.Up()};
            CameraForward.Normalize();
            PlaneUp.Normalize();

            FVector3 PlaneRight{PlaneUp.Cross(CameraForward)};
            PlaneRight.Normalize();

            const float WorldUnitsPerPixel{mOrthographicWidth / static_cast<float>(mWidth)};
            const FVector3 Offset{PlaneRight * (-NavigationInput.mDragDeltaX * WorldUnitsPerPixel) + PlaneUp * (-NavigationInput.mDragDeltaY * WorldUnitsPerPixel)};
            mCameraPosition = mCameraPosition + Offset;
            mOrthographicTarget = mOrthographicTarget + Offset;
        }
        return;
    }

    if (NavigationInput.mDragDeltaX == 0.0f && NavigationInput.mDragDeltaY == 0.0f) {
        return;
    }

    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};
    const float RotationSensitivity{Settings.mRotationSensitivity * 0.001f};
    constexpr float MaximumForwardUp{0.99f};
    const FTransform CameraTransform{FVector3{}, mCameraRotation, FVector3{1.0f, 1.0f, 1.0f}};
    FVector3 Forward{CameraTransform.ToMatrixNoScale().Forward()};
    Forward.Normalize();

    const float CurrentYaw{std::atan2(Forward.mY, Forward.mX)};
    const float CurrentElevation{std::asin(std::clamp(Forward.mZ, -1.0f, 1.0f))};
    const float MaximumElevation{std::asin(MaximumForwardUp)};
    const float NewYaw{CurrentYaw + NavigationInput.mDragDeltaX * RotationSensitivity};
    const float NewElevation{std::clamp(CurrentElevation + NavigationInput.mDragDeltaY * RotationSensitivity, -MaximumElevation, MaximumElevation)};

    const FQuat YawRotation{FQuat::CreateFromAxisAngle(FVector3::UnitZ, NewYaw)};
    const FQuat PitchRotation{FQuat::CreateFromAxisAngle(FVector3::UnitY, -NewElevation)};
    mCameraRotation = FQuat::Concatenate(YawRotation, PitchRotation);
    mCameraRotation.Normalize();
}

void FEditorViewport::ApplyKeyboardNavigation(const FViewportKeyboardNavigationInput& NavigationInput) {
    if (mProjectionType == EProjectionType::Orthographic || NavigationInput.mDeltaTime <= 0.0f ||
        (NavigationInput.mForwardAxis == 0.0f && NavigationInput.mRightAxis == 0.0f && NavigationInput.mUpAxis == 0.f)) {
        return;
    }

    const FTransform CameraTransform{mCameraPosition, mCameraRotation, FVector3{1.0f, 1.0f, 1.0f}};
    const FMatrix CameraWorldMatrix{CameraTransform.ToMatrixNoScale()};
    FVector3 MoveDirection{CameraWorldMatrix.Forward() * NavigationInput.mForwardAxis + CameraWorldMatrix.Right() * NavigationInput.mRightAxis + CameraWorldMatrix.Up() * NavigationInput.mUpAxis};

    if (MoveDirection.LengthSquared() <= 0.0f) {
        return;
    }

    MoveDirection.Normalize();
    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};
    const float MoveSensitivity{Settings.mMoveSensitivity};
    mCameraPosition = mCameraPosition + MoveDirection * MoveSensitivity * NavigationInput.mDeltaTime;
}

FSceneRenderSurface& FEditorViewport::GetRenderSurface() {
    return mRenderSurface;
}

const D3D11_VIEWPORT& FEditorViewport::GetRenderViewport() const {
    return mRenderSurface.GetViewport();
}

const FRenderSettings& FEditorViewport::GetRenderSettings() const {
    return mRenderSettings;
}

const FVector3& FEditorViewport::GetCameraPosition() const {
    return mCameraPosition;
}

void FEditorViewport::SetProjectionType(EProjectionType InProjectionType) {
    if (mProjectionType == InProjectionType) {
        return;
    }

    if (InProjectionType == EProjectionType::Perspective) {
        mProjectionType = EProjectionType::Perspective;
        mCameraPosition = mPerspectiveCameraPosition;
        mCameraRotation = mPerspectiveCameraRotation;
    } else {
        mPerspectiveCameraPosition = mCameraPosition;
        mPerspectiveCameraRotation = mCameraRotation;
        mProjectionType = EProjectionType::Orthographic;
        ApplyOrthographicView();
    }
    mRenderSettings.mBRenderSky = mProjectionType == EProjectionType::Perspective;
}

void FEditorViewport::SetOrthographicView(EOrthographicView InView) {
    if (mProjectionType == EProjectionType::Perspective) {
        mPerspectiveCameraPosition = mCameraPosition;
        mPerspectiveCameraRotation = mCameraRotation;
    }

    mProjectionType = EProjectionType::Orthographic;
    mOrthographicView = InView;
    ApplyOrthographicView();
    mRenderSettings.mBRenderSky = false;
}

void FEditorViewport::SetCameraParameter(const FVector3& InPosition, const FQuat& InRotation, float InFieldOfView, float InOrthographicWidth, float InNearPlane, float InFarPlane) {
    mCameraPosition = InPosition;
    mCameraRotation = InRotation;
    mCameraRotation.Normalize();
    mFieldOfView = InFieldOfView;
    mOrthographicWidth = InOrthographicWidth;
    mNearPlane = InNearPlane;
    mFarPlane = InFarPlane;

    if (mProjectionType == EProjectionType::Perspective) {
        mPerspectiveCameraPosition = mCameraPosition;
        mPerspectiveCameraRotation = mCameraRotation;
    } else {
        const FTransform CameraTransform{mCameraPosition, mCameraRotation, FVector3{1.0f, 1.0f, 1.0f}};
        FVector3 CameraForward{CameraTransform.ToMatrixNoScale().Forward()};
        CameraForward.Normalize();
        const float CameraDistance{(mNearPlane + mFarPlane) * 0.5f};
        mOrthographicTarget = mCameraPosition + CameraForward * CameraDistance;
    }
}

void FEditorViewport::ApplyOrthographicView() {
    mCameraRotation = GetOrthographicRotation(mOrthographicView);
    mCameraRotation.Normalize();

    const FTransform RotationTransform{FVector3{}, mCameraRotation, FVector3{1.0f, 1.0f, 1.0f}};
    FVector3 CameraForward{RotationTransform.ToMatrixNoScale().Forward()};
    CameraForward.Normalize();
    const float CameraDistance{(mNearPlane + mFarPlane) * 0.5f};
    mCameraPosition = mOrthographicTarget - CameraForward * CameraDistance;
}

const char* FEditorViewport::GetViewModeName() const {
    return mProjectionType == EProjectionType::Perspective ? "Perspective" : GetOrthographicViewName(mOrthographicView);
}

void FEditorViewport::ReleaseRenderResources() {
    mRenderSurface.Reset();
}

D3D11_VIEWPORT FEditorViewport::BuildInputViewport() const {
    return D3D11_VIEWPORT{mRenderLeft, mRenderTop, static_cast<float>(mWidth), static_cast<float>(mHeight), 0.0f, 1.0f};
}
