#pragma once

#include <array>
#include <cstdint>
#include <d3d11.h>
#include <optional>

#include "Asset/FAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"
#include "../../Core/Base/FRenderProbe.h"
#include "../../Core/Base/TObjectRef.h"
#include "../../Core/Channel/FStateChannel.h"
#include "World/FWorldEditorContext.h"
#include "World/Component/USceneComponent.h"
#include "Asset/Pipeline/UPipeline.h"

#include "Editor/Input/FMouseInput.h"
#include "Editor/Input/FKeyboardInput.h"
#include "World/Component/UPrimitiveComponent.h"

class FTransformGizmo {
private:
    enum class EAxis : std::uint8_t {
        None,
        X,
        Y,
        Z
    };

    enum class EModifyMode : Uint8 {
        Translate,
        Rotate,
        Scale,
        None
    };

    struct FAxisHitProxy {
        EAxis mAxis{EAxis::None};
        FVector3 mCenter{};
        FVector3 mExtent{};
    };

    struct FAxisHit {
        EAxis mAxis{EAxis::None};
        float mDistance{0.0f};
    };

    struct FDragSession {
        TObjectRef<USceneComponent> mTarget{};
        FVector3 mAxisWorld{};
        FVector3 mInteractionPivotWorld{};
        FVector3 mDragPlaneNormal{};
        float mPreviousAxisParameter{0.0f};
        EAxis mDragAxis{EAxis::None};
        EModifyMode mModifyMode{EModifyMode::None};
        EGizmoCoordinateSpace mCoordinateSpace{EGizmoCoordinateSpace::World};
        FVector3 mPreviousRotationDirection{};
        float mWorkUnitsPerPixel{1.0f};
        float mAccumulatedDelta{0.0f};
    };

public:
    FTransformGizmo() = default;
    ~FTransformGizmo() = default;

    FTransformGizmo(const FTransformGizmo&) = delete;
    FTransformGizmo& operator=(const FTransformGizmo&) = delete;

    FTransformGizmo(FTransformGizmo&&) = default;
    FTransformGizmo& operator=(FTransformGizmo&&) = default;

public:
    void Initialize(ID3D11Device* Device, FAssetRegistry& AssetRegistry, FWorldEditorContext& InEditorContext);

    void ProcessInput(FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, bool BMouseCapturedByUi);
    void Update(const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport);
    void BuildRenderProbes(FRenderProbe& Probe);

    FStateChannel<Uint8>::FReadWriter GetGizmoMode();

    FStateChannel<Uint8>::FReadWriter GetGizmoCoordinateSpace();

private:
    void SetTranslate(const FVector3& Pivot, float WorldUnitsPerPixel);
    void SetScale(const FVector3& Pivot, float WorldUnitsPerPixel);
    void SetRotate(const FVector3& Pivot, float WorldUnitsPerPixel);

    FAssetHandle GetAxisMaterial(EAxis Axis) const;
    void AddRenderPart(EAxis Axis, const FMatrix& LocalTransform, FAssetHandle MeshHandle);

    void UpdateBoundsInGizmoSpace(const UPrimitiveComponent& Primitive, FVector3& OutCenter, FVector3& OutExtent) const;

    std::optional<FRay> MakeWorldRay(const POINT& ScreenPosition) const;
    std::optional<FAxisHit> HitTest(const FRay& WorldRay) const;

    bool BeginDrag(EAxis Axis, const FRay& WorldRay);
    void UpdateDrag(const FRay& WorldRay);
    void EndDrag();
    void RefreshAssetHandles();
    bool GetAxisParameterOnDragPlane(const FRay& WorldRay, const FDragSession& Session, float& OutParameter) const;
    FVector3 GetWorldAxis(EAxis Axis) const;

private:
    static constexpr float ShaftLengthPixels{66.0f};
    static constexpr float ConeLengthPixels{22.0f};
    static constexpr float ShaftRadiusPixels{6.0f};
    static constexpr float ConeRadiusPixels{13.0f};
    static constexpr float PickRadiusPixels{10.0f};
    static constexpr float BoundsGapPixels{5.0f};

    float mCurrentRingRadius{0.0f};
    float mCurrentRingPickHalfWidth{0.0f};

    FAssetHandle mCylinderMesh{};
    FAssetHandle mConeMesh{};
    FAssetHandle mCubeMesh{};
    FAssetHandle mGizmoTorusMesh{};

    FAssetHandle mRedMaterial{};
    FAssetHandle mGreenMaterial{};
    FAssetHandle mBlueMaterial{};

    FAssetHandle mGizmoPipeline{};

    FMatrix mCylinderXAxisTransform{FMatrix::Identity};
    FMatrix mCylinderYAxisTransform{FMatrix::Identity};
    FMatrix mCylinderZAxisTransform{FMatrix::Identity};

    FMatrix mConeXAxisTransform{FMatrix::Identity};
    FMatrix mConeYAxisTransform{FMatrix::Identity};
    FMatrix mConeZAxisTransform{FMatrix::Identity};

    FMatrix mCubeXAxisTransform{FMatrix::Identity};
    FMatrix mCubeYAxisTransform{FMatrix::Identity};
    FMatrix mCubeZAxisTransform{FMatrix::Identity};

    FMatrix mTorusXAxisTransform{FMatrix::Identity};
    FMatrix mTorusYAxisTransform{FMatrix::Identity};
    FMatrix mTorusZAxisTransform{FMatrix::Identity};

    FMatrix mGizmoWorldTransform{FMatrix::Identity};

    FVector3 mBoundsCenterInGizmoSpace{};
    std::array<FAxisHitProxy, 3> mAxisHitProxies{};

    FAssetRegistry* mAssetRegistry{nullptr};
    FWorldEditorContext* mEditorContext{nullptr};
    FStateChannel<Uint8> mGizmoModeChannel{};
    FStateChannel<Uint8>::FReadWriter mGizmoMode{};
    FStateChannel<Uint8> mGizmoCoordinateSpaceChannel{};
    FStateChannel<Uint8>::FReadWriter mGizmoCoordinateSpace{};

    CameraProbe mLastCamera{};
    D3D11_VIEWPORT mLastViewport{};

    std::optional<FDragSession> mDragSession{};

    bool mBVisible{false};
    bool mBHasCamera{false};

    float mCurrentWorkUnitsPerPixel{1.0f};
};
