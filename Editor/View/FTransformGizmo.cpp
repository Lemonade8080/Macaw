#include "pch.h"

#include "FTransformGizmo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <DirectXCollision.h>
#include <limits>

#include "Asset/BasicGeometry/Corn.h"
#include "Asset/BasicGeometry/Cylinder.h"
#include "World/Component/UPrimitiveComponent.h"
#include "World/UWorld.h"

void FTransformGizmo::Initialize(ID3D11Device* Device, FAssetRegistry& AssetRegistry, FWorldEditorContext& InEditorContext) {
    this->mAssetRegistry = &AssetRegistry;
    RefreshAssetHandles();

    mEditorContext = &InEditorContext;

    mGizmoMode = mGizmoModeChannel.GetReadWriter();
    mGizmoMode.Emplace(static_cast<Uint8>(EModifyMode::Translate));
    mGizmoCoordinateSpace = mGizmoCoordinateSpaceChannel.GetReadWriter();
    mGizmoCoordinateSpace.Emplace(static_cast<Uint8>(EGizmoCoordinateSpace::World));
}

void FTransformGizmo::ProcessInput(FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, bool BMouseCapturedByUi) {
    if (KeyboardInput.GetKeyState(VK_SPACE) == EKeyState::Pressed) {
        mGizmoMode.Emplace((mGizmoModeChannel.GetReader().Read() + 1) % 3);
    }

    if (KeyboardInput.GetKeyState(VK_TAB) == EKeyState::Pressed) {
        mGizmoCoordinateSpace.Emplace((mGizmoCoordinateSpaceChannel.GetReader().Read() + 1) % 2);
    }

    const EKeyState LeftState{MouseInput.GetKeyState(Left)};
    const FMouseInput::DragCapture& Capture{MouseInput.GetDragCapture(Left)};

    if (mDragSession.has_value()) {
        if (LeftState == EKeyState::Down) {
            if (const std::optional<FRay> Ray{MakeWorldRay(Capture.mCurrent)}) {
                UpdateDrag(*Ray);
            }
        } else if (LeftState == EKeyState::Released) {
            if (const std::optional<FRay> Ray{MakeWorldRay(Capture.mCurrent)}) {
                UpdateDrag(*Ray);
            }
            EndDrag();
        }

        return;
    }

    if (BMouseCapturedByUi || LeftState != EKeyState::Pressed || !mBVisible) {
        return;
    }

    const std::optional<FRay> Ray{MakeWorldRay(Capture.mStart)};
    if (!Ray.has_value()) {
        return;
    }

    const std::optional<FAxisHit> Hit{HitTest(*Ray)};
    if (!Hit.has_value() || !BeginDrag(Hit->mAxis, *Ray)) {
        return;
    }

    MouseInput.Consume(Left);

    if (Capture.mCurrent.x != Capture.mStart.x || Capture.mCurrent.y != Capture.mStart.y) {
        if (const std::optional<FRay> CurrentRay{MakeWorldRay(Capture.mCurrent)}) {
            UpdateDrag(*CurrentRay);
        }
    }
}

void FTransformGizmo::Update(const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport) {
    mLastCamera = Camera;
    mLastViewport = Viewport;
    mBHasCamera = true;

    if (mEditorContext == nullptr) {
        if (mDragSession.has_value()) {
            EndDrag();
        }
        mBVisible = false;
        return;
    }

    USceneComponent* Target{mEditorContext->GetSelectedTransformTarget()};
    if (Target == nullptr) {
        mBVisible = false;
        return;
    }

    if (mDragSession.has_value() && mDragSession->mTarget.Get() != Target) {
        EndDrag();
    }

    const FMatrix TargetWorld{Target->GetComponentToWorld()};
    const EGizmoCoordinateSpace CoordinateSpace{mGizmoCoordinateSpace.HasValue() ? static_cast<EGizmoCoordinateSpace>(mGizmoCoordinateSpace.Peek()) : EGizmoCoordinateSpace::World};
    const EModifyMode CurrentMode{mGizmoMode.HasValue() ? static_cast<EModifyMode>(mGizmoMode.Peek()) : EModifyMode::None};

    mGizmoWorldTransform = FMatrix::Identity;
    if (CoordinateSpace == EGizmoCoordinateSpace::Local && CurrentMode == EModifyMode::Rotate) {
        const FMatrix TargetRotation{Target->GetComponentTransform().ToMatrixNoScale()};
        for (Uint32 Row{0}; Row < 3; ++Row) {
            for (Uint32 Column{0}; Column < 3; ++Column) {
                mGizmoWorldTransform.m_[Row][Column] = TargetRotation.m_[Row][Column];
            }
        }
    }

    mGizmoWorldTransform.Translation(TargetWorld.Translation());

    FVector3 BoundsExtent{};

    const UPrimitiveComponent* TargetPrimitive{Target->GetTypeInfo()->IsA<UPrimitiveComponent>() ? static_cast<const UPrimitiveComponent*>(Target) : nullptr};

    if (TargetPrimitive != nullptr) {
        UpdateBoundsInGizmoSpace(*TargetPrimitive, mBoundsCenterInGizmoSpace, BoundsExtent);
    } else {
        mBoundsCenterInGizmoSpace = FVector3::Zero;
    }
    const float ViewportHeight{Viewport.Height};
    const float ProjectionYScale{Camera.mProjection.m_[1][1]};
    const FVector3 BoundsCenterWorld{FVector3::Transform(mBoundsCenterInGizmoSpace, mGizmoWorldTransform)};
    const float ViewDepth{FVector3::Transform(BoundsCenterWorld, Camera.mView).mZ};

    if (ViewportHeight <= 0.0f || std::abs(ProjectionYScale) <= std::numeric_limits<float>::epsilon() || ViewDepth <= 0.0f) {
        mBVisible = false;
        return;
    }

    const bool BPerspectiveProjection{std::abs(Camera.mProjection.m_[2][3]) > std::numeric_limits<float>::epsilon()};
    const float WorldUnitsPerPixel{BPerspectiveProjection ? (2.0f * ViewDepth) / (ViewportHeight * ProjectionYScale) : 2.0f / (ViewportHeight * ProjectionYScale)};
    if (!std::isfinite(WorldUnitsPerPixel) || WorldUnitsPerPixel <= 0.0f) {
        mBVisible = false;
        return;
    }
    mCurrentWorkUnitsPerPixel = WorldUnitsPerPixel;

    switch (CurrentMode) {
        case EModifyMode::Translate:
            SetTranslate(mBoundsCenterInGizmoSpace, WorldUnitsPerPixel);
            break;

        case EModifyMode::Scale:
            SetScale(mBoundsCenterInGizmoSpace, WorldUnitsPerPixel);
            break;

        case EModifyMode::Rotate:
            SetRotate(mBoundsCenterInGizmoSpace, WorldUnitsPerPixel);
            break;

        default:
            mBVisible = false;
            return;
    }

    mBVisible = true;
}

void FTransformGizmo::SetTranslate(const FVector3& Pivot, float WorldUnitsPerPixel) {
    const float ShaftLength{ShaftLengthPixels * WorldUnitsPerPixel};
    const float ConeLength{ConeLengthPixels * WorldUnitsPerPixel};
    const float ShaftRadius{ShaftRadiusPixels * WorldUnitsPerPixel};
    const float ConeRadius{ConeRadiusPixels * WorldUnitsPerPixel};
    const float PickRadius{PickRadiusPixels * WorldUnitsPerPixel};
    const float BoundsGap{BoundsGapPixels * WorldUnitsPerPixel};

    const float HalfShaftLength{ShaftLength * 0.5f};
    const float HalfConeLength{ConeLength * 0.5f};
    const float TotalLength{ShaftLength + ConeLength};

    const float StartX{Pivot.mX + BoundsGap};
    const float StartY{Pivot.mY + BoundsGap};
    const float StartZ{Pivot.mZ + BoundsGap};

    mCylinderXAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ShaftRadius, ShaftRadius, ShaftLength) * FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateTranslation(StartX + HalfShaftLength, Pivot.mY, Pivot.mZ);

    mCylinderYAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ShaftRadius, ShaftRadius, ShaftLength) * FMatrix::CreateRotationX(DirectX::XMConvertToRadians(-90.0f)) * FMatrix::CreateTranslation(Pivot.mX, StartY + HalfShaftLength, Pivot.mZ);

    mCylinderZAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ShaftRadius, ShaftRadius, ShaftLength) * FMatrix::CreateTranslation(Pivot.mX, Pivot.mY, StartZ + HalfShaftLength);

    mConeXAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ConeRadius, ConeLength, ConeRadius) * FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateTranslation(StartX + ShaftLength * 0.8f + HalfConeLength, Pivot.mY, Pivot.mZ);

    mConeYAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ConeRadius, ConeLength, ConeRadius) * FMatrix::CreateRotationX(DirectX::XMConvertToRadians(-90.f)) * FMatrix::CreateTranslation(Pivot.mX, StartY + ShaftLength * 0.8f + HalfConeLength, Pivot.mZ);

    mConeZAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ConeRadius, ConeLength, ConeRadius) * FMatrix::CreateRotationX(DirectX::XMConvertToRadians(0.f)) * FMatrix::CreateTranslation(Pivot.mX, Pivot.mY, StartZ + ShaftLength * 0.8f + HalfConeLength);

    mAxisHitProxies = { FAxisHitProxy{ .mAxis = EAxis::X, .mCenter = FVector3{StartX + TotalLength * 0.5f, Pivot.mY, Pivot.mZ}, .mExtent = FVector3{TotalLength * 0.5f, PickRadius, PickRadius}}, FAxisHitProxy{ .mAxis = EAxis::Y, .mCenter = FVector3{Pivot.mX, StartY + TotalLength * 0.5f, Pivot.mZ}, .mExtent = FVector3{PickRadius, TotalLength * 0.5f, PickRadius}}, FAxisHitProxy{ .mAxis = EAxis::Z, .mCenter = FVector3{Pivot.mX, Pivot.mY, StartZ + TotalLength * 0.5f}, .mExtent = FVector3{PickRadius, PickRadius, TotalLength * 0.5f}}};
}

void FTransformGizmo::SetRotate(const FVector3& Pivot, float WorldUnitsPerPixel) {
    constexpr float RingOuterRadiusPixels{76.0f};
    constexpr float RingPickThicknessPixels{8.0f};
    constexpr float MeshOuterRadius{0.50f};
    constexpr float MeshCenterRadius{0.49f};

    const float RingOuterRadius{RingOuterRadiusPixels * WorldUnitsPerPixel};
    const float RingPickThickness{RingPickThicknessPixels * WorldUnitsPerPixel};

    mCurrentRingRadius = RingOuterRadius * (MeshCenterRadius / MeshOuterRadius);
    mCurrentRingPickHalfWidth = RingPickThickness;

    const float TorusScale{RingOuterRadius / MeshOuterRadius};

    mTorusXAxisTransform = FMatrix::CreateScale(TorusScale, TorusScale, TorusScale) * FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateTranslation(Pivot);

    mTorusYAxisTransform = FMatrix::CreateScale(TorusScale, TorusScale, TorusScale) * FMatrix::CreateRotationX(DirectX::XMConvertToRadians(-90.0f)) * FMatrix::CreateTranslation(Pivot);

    mTorusZAxisTransform = FMatrix::CreateScale(TorusScale, TorusScale, TorusScale) * FMatrix::CreateTranslation(Pivot);
}

void FTransformGizmo::SetScale(const FVector3& Pivot, float WorldUnitsPerPixel) {
    constexpr float ScaleBoxSizePixels{18.0f};

    const float ShaftLength{ShaftLengthPixels * WorldUnitsPerPixel};
    const float ShaftRadius{ShaftRadiusPixels * WorldUnitsPerPixel};
    const float BoxSize{ScaleBoxSizePixels * WorldUnitsPerPixel};
    const float PickRadius{PickRadiusPixels * WorldUnitsPerPixel};
    const float BoundsGap{BoundsGapPixels * WorldUnitsPerPixel};

    const float HalfShaftLength{ShaftLength * 0.5f};
    const float HalfBoxSize{BoxSize * 0.5f};
    const float TotalLength{ShaftLength + BoxSize};

    const float StartX{Pivot.mX + BoundsGap};
    const float StartY{Pivot.mY + BoundsGap};
    const float StartZ{Pivot.mZ + BoundsGap};

    mCylinderXAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ShaftRadius, ShaftLength, ShaftRadius) * FMatrix::CreateRotationZ(DirectX::XMConvertToRadians(-90.0f)) * FMatrix::CreateTranslation(StartX + HalfShaftLength, Pivot.mY, Pivot.mZ);

    mCylinderYAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ShaftRadius, ShaftLength, ShaftRadius) * FMatrix::CreateTranslation(Pivot.mX, StartY + HalfShaftLength, Pivot.mZ);

    mCylinderZAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(ShaftRadius, ShaftLength, ShaftRadius) * FMatrix::CreateRotationX(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateTranslation(Pivot.mX, Pivot.mY, StartZ + HalfShaftLength);

    mCubeXAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(BoxSize, BoxSize, BoxSize) * FMatrix::CreateTranslation(StartX + ShaftLength + HalfBoxSize, Pivot.mY, Pivot.mZ);

    mCubeYAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(BoxSize, BoxSize, BoxSize) * FMatrix::CreateTranslation(Pivot.mX, StartY + ShaftLength + HalfBoxSize, Pivot.mZ);

    mCubeZAxisTransform = FMatrix::CreateRotationY(DirectX::XMConvertToRadians(90.0f)) * FMatrix::CreateScale(BoxSize, BoxSize, BoxSize) * FMatrix::CreateTranslation(Pivot.mX, Pivot.mY, StartZ + ShaftLength + HalfBoxSize);

    mAxisHitProxies = {FAxisHitProxy{ .mAxis = EAxis::X, .mCenter = FVector3{StartX + TotalLength * 0.5f, Pivot.mY, Pivot.mZ}, .mExtent = FVector3{TotalLength * 0.5f, PickRadius, PickRadius}}, FAxisHitProxy{ .mAxis = EAxis::Y, .mCenter = FVector3{Pivot.mX, StartY + TotalLength * 0.5f, Pivot.mZ}, .mExtent = FVector3{PickRadius, TotalLength * 0.5f, PickRadius}}, FAxisHitProxy{ .mAxis = EAxis::Z, .mCenter = FVector3{Pivot.mX, Pivot.mY, StartZ + TotalLength * 0.5f}, .mExtent = FVector3{PickRadius, PickRadius, TotalLength * 0.5f}}};
}

void FTransformGizmo::UpdateBoundsInGizmoSpace(const UPrimitiveComponent& Primitive, FVector3& OutCenter, FVector3& OutExtent) const {
    DirectX::BoundingOrientedBox LocalBounds{};
    LocalBounds = Primitive.GetPickingBox();

    std::array<DirectX::XMFLOAT3, DirectX::BoundingOrientedBox::CORNER_COUNT> Corners{};
    LocalBounds.GetCorners(Corners.data());

    const FMatrix PrimitiveToGizmo{Primitive.GetComponentToWorld() * mGizmoWorldTransform.Invert()};
    FVector3 Minimum{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    FVector3 Maximum{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

    for (const DirectX::XMFLOAT3& Corner : Corners) {
        const FVector3 PointInGizmoSpace{FVector3::Transform(FVector3{Corner}, PrimitiveToGizmo)};
        Minimum = FVector3::Min(Minimum, PointInGizmoSpace);
        Maximum = FVector3::Max(Maximum, PointInGizmoSpace);
    }

    OutCenter = (Minimum + Maximum) * 0.5f;
    OutExtent = (Maximum - Minimum) * 0.5f;
}

std::optional<FRay> FTransformGizmo::MakeWorldRay(const POINT& ScreenPosition) const {
    if (!mBHasCamera) {
        return std::nullopt;
    }

    const D3D11_VIEWPORT& Viewport{mLastViewport};
    if (Viewport.Width <= 0.0f || Viewport.Height <= 0.0f) {
        return std::nullopt;
    }

    const float ViewportX{static_cast<float>(ScreenPosition.x) - Viewport.TopLeftX};
    const float ViewportY{static_cast<float>(ScreenPosition.y) - Viewport.TopLeftY};
    const float NdcX{2.0f * ViewportX / Viewport.Width - 1.0f};
    const float NdcY{1.0f - 2.0f * ViewportY / Viewport.Height};

    const FMatrix InverseViewProjection{mLastCamera.mViewProjection.Invert()};
    const FVector3 RayOrigin{FVector3::Transform(FVector3{NdcX, NdcY, 0.0f}, InverseViewProjection)};
    FVector3 RayDirection{FVector3::Transform(FVector3{NdcX, NdcY, 1.0f}, InverseViewProjection) - RayOrigin};

    if (RayDirection.LengthSquared() <= std::numeric_limits<float>::epsilon()) {
        return std::nullopt;
    }

    RayDirection.Normalize();
    return FRay{RayOrigin.ToSimpleMath(), RayDirection.ToSimpleMath()};
}

std::optional<FTransformGizmo::FAxisHit> FTransformGizmo::HitTest(const FRay& WorldRay) const {
    const FMatrix InverseGizmoWorld{mGizmoWorldTransform.Invert()};
    const FVector3 LocalOrigin{FVector3::Transform(FVector3{WorldRay.position}, InverseGizmoWorld)};
    FVector3 LocalDirection{FVector3::TransformNormal(FVector3{WorldRay.direction}, InverseGizmoWorld)};
    if (LocalDirection.LengthSquared() <= std::numeric_limits<float>::epsilon()) {
        return std::nullopt;
    }
    LocalDirection.Normalize();

    const FRay LocalRay{LocalOrigin.ToSimpleMath(), LocalDirection.ToSimpleMath()};
    std::optional<FAxisHit> NearestHit{};
    const EModifyMode CurrentMode{mGizmoMode.HasValue() ? static_cast<EModifyMode>(mGizmoMode.Peek()) : EModifyMode::None};

    if (CurrentMode == EModifyMode::Rotate) {
        const EAxis Axis[3]{EAxis::X, EAxis::Y, EAxis::Z};
        const FVector3 PlaneNormals[3]{FVector3::UnitX, FVector3::UnitY, FVector3::UnitZ};
        for (int I{0}; I < 3; I++) {
            FVector3 PlaneNormal{PlaneNormals[I]};
            float Denominator{LocalDirection.Dot(PlaneNormal)};
            if (std::abs(Denominator) <= 0.000001f) {
                continue;
            }
            float Distance{(mBoundsCenterInGizmoSpace - LocalOrigin).Dot(PlaneNormal) / Denominator};
            if (Distance < 0.0f) {
                continue;
            }
            FVector3 HitPosition{LocalOrigin + LocalDirection * Distance};
            float DistanceFromPivot{(HitPosition - mBoundsCenterInGizmoSpace).Length()};
            float DistanceFromRadius{std::abs(DistanceFromPivot - mCurrentRingRadius)};
            if (DistanceFromRadius <= mCurrentRingPickHalfWidth) {
                if (!NearestHit.has_value() || Distance < NearestHit->mDistance) {
                    NearestHit = FAxisHit{ .mAxis = Axis[I], .mDistance = Distance};
                }
            }
        }
        return NearestHit;
    }

    for (const FAxisHitProxy& Proxy : mAxisHitProxies) {
        const DirectX::BoundingBox Box{Proxy.mCenter.ToSimpleMath(), Proxy.mExtent.ToSimpleMath()};
        float Distance{0.0f};
        if (Box.Intersects(LocalRay.position, LocalRay.direction, Distance) && (!NearestHit.has_value() || Distance < NearestHit->mDistance)) {
            NearestHit = FAxisHit{ .mAxis = Proxy.mAxis, .mDistance = Distance};
        }
    }

    return NearestHit;
}

bool FTransformGizmo::BeginDrag(EAxis Axis, const FRay& WorldRay) {

    if (mEditorContext == nullptr || Axis == EAxis::None) {
        return false;
    }

    const EModifyMode CurrentMode{mGizmoMode.HasValue() ? static_cast<EModifyMode>(mGizmoMode.Peek()) : EModifyMode::None};

    if (CurrentMode == EModifyMode::None) {
        return false;
    }

    FVector3 AxisWorld{GetWorldAxis(Axis)};

    if (AxisWorld.LengthSquared() <= std::numeric_limits<float>::epsilon()) {
        return false;
    }

    AxisWorld.Normalize();

    const FVector3 InteractionPivotWorld{FVector3::Transform(mBoundsCenterInGizmoSpace, mGizmoWorldTransform)};

    FDragSession NewSession{};

    USceneComponent* Target{mEditorContext->GetSelectedTransformTarget()};
    if (Target == nullptr) {
        return false;
    }

    NewSession.mTarget.Set(Target);
    NewSession.mModifyMode = CurrentMode;
    NewSession.mCoordinateSpace = mGizmoCoordinateSpace.HasValue() ? static_cast<EGizmoCoordinateSpace>(mGizmoCoordinateSpace.Peek()) : EGizmoCoordinateSpace::World;
    NewSession.mDragAxis = Axis;
    NewSession.mAxisWorld = AxisWorld;
    NewSession.mInteractionPivotWorld = InteractionPivotWorld;
    NewSession.mWorkUnitsPerPixel = mCurrentWorkUnitsPerPixel;
    NewSession.mAccumulatedDelta = 0.0f;

    if (CurrentMode == EModifyMode::Rotate) {

        NewSession.mDragPlaneNormal = AxisWorld;

        const FPlane RotationPlane{ InteractionPivotWorld.ToSimpleMath(), AxisWorld.ToSimpleMath()};

        float Distance{0.0f};

        if (!WorldRay.Intersects(RotationPlane, Distance) || Distance < 0.0f) {
            return false;
        }

        const FVector3 HitPosition{ WorldRay.position + WorldRay.direction * Distance};

        FVector3 InitialDirection{HitPosition - InteractionPivotWorld};

        InitialDirection = InitialDirection - AxisWorld * InitialDirection.Dot(AxisWorld);

        if (InitialDirection.LengthSquared() <= 0.000001f) {
            return false;
        }

        InitialDirection.Normalize();

        NewSession.mPreviousRotationDirection = InitialDirection;
    }

    else {
        FVector3 ViewDirection{WorldRay.direction};
        if (ViewDirection.LengthSquared() <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        ViewDirection.Normalize();

        FVector3 PlaneNormal{ViewDirection - AxisWorld * ViewDirection.Dot(AxisWorld)};

        constexpr float MinimumViewSeparation{0.05f};
        if (PlaneNormal.LengthSquared() <= MinimumViewSeparation * MinimumViewSeparation) {
            return false;
        }

        PlaneNormal.Normalize();

        NewSession.mDragPlaneNormal = PlaneNormal;

        if (!GetAxisParameterOnDragPlane(WorldRay, NewSession, NewSession.mPreviousAxisParameter)) {
            return false;
        }
    }

    mDragSession = NewSession;

    return true;
}

void FTransformGizmo::UpdateDrag(const FRay& WorldRay) {
    if (!mDragSession.has_value()) {
        return;
    }

    auto& Session{*mDragSession};
    USceneComponent* Target{Session.mTarget.Get()};
    if (mEditorContext == nullptr || Target == nullptr || mEditorContext->GetSelectedTransformTarget() != Target) {
        EndDrag();
        return;
    }

    if (Session.mModifyMode == EModifyMode::Rotate) {

        const FPlane RotationPlane{ Session.mInteractionPivotWorld.ToSimpleMath(), Session.mAxisWorld.ToSimpleMath()};

        float Distance{0.0f};

        if (!WorldRay.Intersects(RotationPlane, Distance) || Distance < 0.0f) {
            return;
        }

        const FVector3 HitPosition{ WorldRay.position + WorldRay.direction * Distance};

        FVector3 CurrentDirection{HitPosition - Session.mInteractionPivotWorld};

        CurrentDirection = CurrentDirection - Session.mAxisWorld * CurrentDirection.Dot(Session.mAxisWorld);

        if (CurrentDirection.LengthSquared() <= 0.000001f) {
            return;
        }

        CurrentDirection.Normalize();

        const float SinAngle{Session.mAxisWorld.Dot(Session.mPreviousRotationDirection.Cross(CurrentDirection))};
        const float CosAngle{std::clamp(Session.mPreviousRotationDirection.Dot(CurrentDirection), -1.0f, 1.0f)};
        const float AngleDelta{std::atan2(SinAngle, CosAngle)};
        if (Session.mCoordinateSpace == EGizmoCoordinateSpace::Local) {
            FVector3 LocalAxis{};
            switch (Session.mDragAxis) {
                case EAxis::X:
                    LocalAxis = FVector3::UnitX;
                    break;
                case EAxis::Y:
                    LocalAxis = FVector3::UnitY;
                    break;
                case EAxis::Z:
                    LocalAxis = FVector3::UnitZ;
                    break;
                default:
                    return;
            }

            FTransform RelativeTransform{Target->GetRelativeTransform()};
            RelativeTransform.SetRotation(FQuat::Concatenate(RelativeTransform.GetRotationQuaternion(), FQuat::CreateFromAxisAngle(LocalAxis, AngleDelta)));
            Target->SetRelativeTransform(RelativeTransform);
            Session.mPreviousRotationDirection = CurrentDirection;
            return;
        }

        const FVector3 TransformSpaceAxis{Session.mAxisWorld};
        FTransform DesiredWorldTransform{Target->GetComponentTransform()};
        if (Session.mCoordinateSpace == EGizmoCoordinateSpace::Local) {
            DesiredWorldTransform.SetRotation(FQuat::Concatenate(DesiredWorldTransform.GetRotationQuaternion(), FQuat::CreateFromAxisAngle(TransformSpaceAxis, AngleDelta)));
        } else {
            DesiredWorldTransform.SetRotation(FQuat::Concatenate(FQuat::CreateFromAxisAngle(TransformSpaceAxis, AngleDelta), DesiredWorldTransform.GetRotationQuaternion()));
        }

        if (Target->SetWorldTransform(DesiredWorldTransform)) {
            Session.mPreviousRotationDirection = CurrentDirection;
        }
        return;
    }

    else {
        float CurrentAxisParameter{0.0f};

        if (!GetAxisParameterOnDragPlane(WorldRay, Session, CurrentAxisParameter)) {
            return;
        }

        const float Delta{CurrentAxisParameter - Session.mPreviousAxisParameter};
        const float MaximumFrameDelta{std::max(Session.mWorkUnitsPerPixel * std::max(mLastViewport.Width, mLastViewport.Height) * 2.0f, 1.0f)};
        if (!std::isfinite(Delta) || std::abs(Delta) > MaximumFrameDelta) {
            EndDrag();
            return;
        }
        Session.mPreviousAxisParameter = CurrentAxisParameter;
        FTransform DesiredWorldTransform{Target->GetComponentTransform()};

        if (Session.mModifyMode == EModifyMode::Translate) {
            const FEditorSettings Settings{mEditorContext->GetEditorSettings()};
            const float GridSize{Settings.mGridSize};

            if (Settings.mGridSnapEnabled && GridSize > 0.0f) {
                Session.mAccumulatedDelta += Delta;
                if (std::abs(Session.mAccumulatedDelta) < GridSize) {
                    return;
                }
                const float Steps{truncf(Session.mAccumulatedDelta / GridSize)};
                const float StepDelta{Steps * GridSize};

                DesiredWorldTransform.SetPosition(DesiredWorldTransform.GetPosition() + Session.mAxisWorld * StepDelta);
                Session.mAccumulatedDelta -= StepDelta;
            } else {
                DesiredWorldTransform.SetPosition(DesiredWorldTransform.GetPosition() + Session.mAxisWorld * Delta);
                Session.mAccumulatedDelta = 0.0f;
            }

        } else if (Session.mModifyMode == EModifyMode::Scale) {
            const float ScaleSpeed{std::max(100.0f * Session.mWorkUnitsPerPixel, 0.0001f)};
            const float ScaleFactor{std::max(0.01f, 1.0f + Delta / ScaleSpeed)};

            if (Session.mCoordinateSpace == EGizmoCoordinateSpace::Local) {
                FVector3 RelativeScale{Target->GetRelativeScale3D()};
                switch (Session.mDragAxis) {
                    case EAxis::X:
                        RelativeScale.mX *= ScaleFactor;
                        break;
                    case EAxis::Y:
                        RelativeScale.mY *= ScaleFactor;
                        break;
                    case EAxis::Z:
                        RelativeScale.mZ *= ScaleFactor;
                        break;
                    default:
                        return;
                }

                Target->SetRelativeScale3D(RelativeScale);
                Session.mPreviousAxisParameter = CurrentAxisParameter;
                return;
            }

            FVector3 CurrentScale{DesiredWorldTransform.GetScale()};

            switch (Session.mDragAxis) {
                case EAxis::X:
                    CurrentScale.mX *= ScaleFactor;
                    break;

                case EAxis::Y:
                    CurrentScale.mY *= ScaleFactor;
                    break;

                case EAxis::Z:
                    CurrentScale.mZ *= ScaleFactor;
                    break;

                default:
                    return;
            }

            DesiredWorldTransform.SetScale(CurrentScale);
        } else {
            return;
        }

        if (Target->SetWorldTransform(DesiredWorldTransform)) {
            Session.mPreviousAxisParameter = CurrentAxisParameter;
        }
    }
}

void FTransformGizmo::EndDrag() {
    if (!mDragSession.has_value()) {
        return;
    }

    mDragSession.reset();
}

bool FTransformGizmo::GetAxisParameterOnDragPlane(const FRay& WorldRay, const FDragSession& Session, float& OutParameter) const {
    const FVector3 RayOrigin{WorldRay.position};
    const FVector3 RayDirection{WorldRay.direction};
    const float Denominator{RayDirection.Dot(Session.mDragPlaneNormal)};
    constexpr float MinimumRayPlaneAlignment{0.05f};
    if (!std::isfinite(Denominator) || std::abs(Denominator) < MinimumRayPlaneAlignment) {
        return false;
    }

    const float Distance{(Session.mInteractionPivotWorld - RayOrigin).Dot(Session.mDragPlaneNormal) / Denominator};
    if (!std::isfinite(Distance) || Distance < 0.0f) {
        return false;
    }

    const FVector3 HitPosition{RayOrigin + RayDirection * Distance};
    const float Parameter{(HitPosition - Session.mInteractionPivotWorld).Dot(Session.mAxisWorld)};
    if (!std::isfinite(Parameter)) {
        return false;
    }

    OutParameter = Parameter;
    return true;
}

FVector3 FTransformGizmo::GetWorldAxis(EAxis Axis) const {
    switch (Axis) {
        case EAxis::X:
            return mGizmoWorldTransform.Right();
        case EAxis::Z:
            return mGizmoWorldTransform.Up();
        case EAxis::Y:
            return mGizmoWorldTransform.Forward();
        default:
            return FVector3::Zero;
    }
}

void FTransformGizmo::BuildRenderProbes(FRenderProbe& Probe) {
    RefreshAssetHandles();

    if (!mBVisible) {
        return;
    }

    const EModifyMode CurrentMode{mGizmoMode.HasValue() ? static_cast<EModifyMode>(mGizmoMode.Peek()) : EModifyMode::None};

    const auto Submit{[&](const FMatrix& LocalTransform, FAssetHandle MeshHandle, FAssetHandle MaterialHandle) {
        Probe.mGizmoProbes.emplace_back(FActorProbe{ .mWorld = LocalTransform * mGizmoWorldTransform, .mMeshHandle = MeshHandle, .mMaterialHandle = MaterialHandle, .mPipelineHandle = mGizmoPipeline, .mFlags = static_cast<Uint32>(ERenderObjectFlags::Unlit)});
    }};

    switch (CurrentMode) {
        case EModifyMode::Translate:
            Submit(mCylinderXAxisTransform, mCylinderMesh, mRedMaterial);
            Submit(mCylinderYAxisTransform, mCylinderMesh, mGreenMaterial);
            Submit(mCylinderZAxisTransform, mCylinderMesh, mBlueMaterial);

            Submit(mConeXAxisTransform, mConeMesh, mRedMaterial);
            Submit(mConeYAxisTransform, mConeMesh, mGreenMaterial);
            Submit(mConeZAxisTransform, mConeMesh, mBlueMaterial);
            break;

        case EModifyMode::Scale:
            Submit(mCylinderXAxisTransform, mCylinderMesh, mRedMaterial);
            Submit(mCylinderYAxisTransform, mCylinderMesh, mGreenMaterial);
            Submit(mCylinderZAxisTransform, mCylinderMesh, mBlueMaterial);

            Submit(mCubeXAxisTransform, mCubeMesh, mRedMaterial);
            Submit(mCubeYAxisTransform, mCubeMesh, mGreenMaterial);
            Submit(mCubeZAxisTransform, mCubeMesh, mBlueMaterial);
            break;

        case EModifyMode::Rotate:
            Submit(mTorusXAxisTransform, mGizmoTorusMesh, mRedMaterial);
            Submit(mTorusYAxisTransform, mGizmoTorusMesh, mGreenMaterial);
            Submit(mTorusZAxisTransform, mGizmoTorusMesh, mBlueMaterial);
            break;

        default:
            break;
    }
}

void FTransformGizmo::RefreshAssetHandles() {
    if (mAssetRegistry == nullptr) {
        return;
    }

    mCylinderMesh = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Mesh/Cylinder.bin"});
    mConeMesh = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Mesh/Cone.bin"});
    mCubeMesh = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Mesh/Cube.bin"});
    mGizmoTorusMesh = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Mesh/GizmoTorus.bin"});
    mRedMaterial = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Red.mtl"});
    mGreenMaterial = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Green.mtl"});
    mBlueMaterial = mAssetRegistry->FindAsset(FAssetPath{"/Game/System/Material/Blue.mtl"});
    mGizmoPipeline = mAssetRegistry->FindAsset(FAssetPath{"/Game/Pipeline/Gizmo.json"});
}

FStateChannel<Uint8>::FReadWriter FTransformGizmo::GetGizmoMode() {
    return mGizmoModeChannel.GetReadWriter();
}

FStateChannel<Uint8>::FReadWriter FTransformGizmo::GetGizmoCoordinateSpace() {
    return mGizmoCoordinateSpaceChannel.GetReadWriter();
}
