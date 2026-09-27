#include "pch.h"

#include "EditorViewport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Editor/Input/FMouseInput.h"

#include "World/Component/UCollisionComponent.h"
#include "World/Component/UBillboardComponent.h"
#include "World/Component/UMeshComponent.h"

namespace {
    bool GetVisibleGridBounds(const CameraProbe& Camera, float& MinimumX, float& MaximumX, float& MinimumY, float& MaximumY) {
        FMatrix InverseViewProjection{};
        if (!Camera.mViewProjection.TryInverse(InverseViewProjection)) {
            return false;
        }

        const std::array<FVector3, 8> ClipCorners{ FVector3{-1.0f, -1.0f, 0.0f}, FVector3{1.0f, -1.0f, 0.0f}, FVector3{1.0f, 1.0f, 0.0f}, FVector3{-1.0f, 1.0f, 0.0f}, FVector3{-1.0f, -1.0f, 1.0f}, FVector3{1.0f, -1.0f, 1.0f}, FVector3{1.0f, 1.0f, 1.0f}, FVector3{-1.0f, 1.0f, 1.0f}};
        constexpr std::array<std::array<int, 2>, 12> Edges{ std::array<int, 2>{0, 1}, std::array<int, 2>{1, 2}, std::array<int, 2>{2, 3}, std::array<int, 2>{3, 0}, std::array<int, 2>{4, 5}, std::array<int, 2>{5, 6}, std::array<int, 2>{6, 7}, std::array<int, 2>{7, 4}, std::array<int, 2>{0, 4}, std::array<int, 2>{1, 5}, std::array<int, 2>{2, 6}, std::array<int, 2>{3, 7}};
        std::array<FVector3, 8> WorldCorners{};
        for (std::size_t Index{0}; Index < ClipCorners.size(); ++Index) {
            if (!InverseViewProjection.TransformCoord(ClipCorners[Index], WorldCorners[Index])) {
                return false;
            }
        }

        bool HasPoint{false};
        const auto IncludePoint{[&MinimumX, &MaximumX, &MinimumY, &MaximumY, &HasPoint](const FVector3& Point) {
            if (!HasPoint) {
                MinimumX = MaximumX = Point.mX;
                MinimumY = MaximumY = Point.mY;
                HasPoint = true;
                return;
            }
            MinimumX = std::min(MinimumX, Point.mX);
            MaximumX = std::max(MaximumX, Point.mX);
            MinimumY = std::min(MinimumY, Point.mY);
            MaximumY = std::max(MaximumY, Point.mY);
        }};
        constexpr float PlaneEpsilon{0.0001f};
        for (const std::array<int, 2>& Edge : Edges) {
            const FVector3& Start{WorldCorners[Edge[0]]};
            const FVector3& End{WorldCorners[Edge[1]]};
            const bool StartOnPlane{std::abs(Start.mZ) <= PlaneEpsilon};
            const bool EndOnPlane{std::abs(End.mZ) <= PlaneEpsilon};
            if (StartOnPlane) {
                IncludePoint(Start);
            }
            if (EndOnPlane) {
                IncludePoint(End);
            }
            if ((Start.mZ < -PlaneEpsilon && End.mZ > PlaneEpsilon) || (Start.mZ > PlaneEpsilon && End.mZ < -PlaneEpsilon)) {
                const float Fraction{-Start.mZ / (End.mZ - Start.mZ)};
                IncludePoint(FVector3{Start.mX + (End.mX - Start.mX) * Fraction, Start.mY + (End.mY - Start.mY) * Fraction, 0.0f});
            }
        }
        return HasPoint;
    }
}

void EditorViewport::Initialize(ID3D11Device* Device, FAssetRegistry& AssetRegistry, FWorldEditorContext& InEditorContext) {
    mTransformGizmo.Initialize(Device, AssetRegistry, InEditorContext);
    mEditorContext = &InEditorContext;
}

void EditorViewport::PrepareInput(const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport) {
    mTransformGizmo.Update(Camera, Viewport);
}

void EditorViewport::ProcessInput(FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, bool BMouseCapturedByUi) {
    mTransformGizmo.ProcessInput(KeyboardInput, MouseInput, BMouseCapturedByUi);
}

FStateChannel<Uint8>::FReadWriter EditorViewport::GetGizmoMode() {
    return mTransformGizmo.GetGizmoMode();
}

FStateChannel<Uint8>::FReadWriter EditorViewport::GetGizmoCoordinateSpace() {
    return mTransformGizmo.GetGizmoCoordinateSpace();
}

void EditorViewport::BuildGrid(FLineRenderData& Lines, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport, FVector2D& FadeCenter, ELineDepthMode DepthMode) {
    const float GridInterval{mEditorContext != nullptr ? mEditorContext->GetEditorSettings().mGridSize : 1.0f};
    const float ProjectionYScale{Camera.mProjection.m_[1][1]};
    if (!std::isfinite(GridInterval) || GridInterval <= 0.0f || ProjectionYScale <= 0.0f || Viewport.Width <= 0.0f || Viewport.Height <= 0.0f) {
        return;
    }
    float MinimumX{};
    float MaximumX{};
    float MinimumY{};
    float MaximumY{};

    if (!GetVisibleGridBounds(Camera, MinimumX, MaximumX, MinimumY, MaximumY)) {
        return;
    }

    const bool Perspective{std::abs(Camera.mProjection.m_[2][3]) > 0.0001f};
    FadeCenter = Perspective ? FVector2D{CameraPosition.mX, CameraPosition.mY} : FVector2D{(MinimumX + MaximumX) * 0.5f, (MinimumY + MaximumY) * 0.5f};
    constexpr float GridRadius{550.0f};
    MinimumX = std::max(MinimumX, FadeCenter.mX - GridRadius);
    MaximumX = std::min(MaximumX, FadeCenter.mX + GridRadius);
    MinimumY = std::max(MinimumY, FadeCenter.mY - GridRadius);
    MaximumY = std::min(MaximumY, FadeCenter.mY + GridRadius);
    if (MinimumX >= MaximumX || MinimumY >= MaximumY) {
        return;
    }
    float Step{GridInterval};
    int Level{0};
    while (((MaximumX - MinimumX) / Step > 2048.0f || (MaximumY - MinimumY) / Step > 2048.0f) && Level < 8) {
        Step *= 10.0f;
        ++Level;
    }

    const float Margin{std::max(2.0f * Step, 3.0f)};
    MinimumX = std::max(MinimumX - Margin, FadeCenter.mX - GridRadius);
    MaximumX = std::min(MaximumX + Margin, FadeCenter.mX + GridRadius);
    MinimumY = std::max(MinimumY - Margin, FadeCenter.mY - GridRadius);
    MaximumY = std::min(MaximumY + Margin, FadeCenter.mY + GridRadius);
    const std::int64_t FirstX{static_cast<std::int64_t>(std::ceil(static_cast<double>(MinimumX) / Step))};
    const std::int64_t LastX{static_cast<std::int64_t>(std::floor(static_cast<double>(MaximumX) / Step))};
    const std::int64_t FirstY{static_cast<std::int64_t>(std::ceil(static_cast<double>(MinimumY) / Step))};
    const std::int64_t LastY{static_cast<std::int64_t>(std::floor(static_cast<double>(MaximumY) / Step))};
    for (std::int64_t Index{FirstX}; Index <= LastX; ++Index) {
        const float TierSpacing{Index % 100 == 0 ? -Step * 100.0f : Index % 10 == 0 ? -Step * 10.0f : Step};
        const float WidthPixels{TierSpacing < 0.0f ? 1.5f : 1.0f};
        const float Position{static_cast<float>(Index) * Step};
        const float Offset{Position - FadeCenter.mX};
        const float HalfLength{std::sqrt(std::max(GridRadius * GridRadius - Offset * Offset, 0.0f))};
        const float Start{std::max(MinimumY, FadeCenter.mY - HalfLength)};
        const float End{std::min(MaximumY, FadeCenter.mY + HalfLength)};
        if (End > Start) {
            Lines.AddGridLine(FVector3{Position, Start, 0.0f}, FVector3{Position, End, 0.0f}, FVector4{0.5f, 0.5f, 0.5f, 1.0f}, WidthPixels, TierSpacing, DepthMode);
        }
    }
    for (std::int64_t Index{FirstY}; Index <= LastY; ++Index) {
        const float TierSpacing{Index % 100 == 0 ? -Step * 100.0f : Index % 10 == 0 ? -Step * 10.0f : Step};
        const float WidthPixels{TierSpacing < 0.0f ? 1.5f : 1.0f};
        const float Position{static_cast<float>(Index) * Step};
        const float Offset{Position - FadeCenter.mY};
        const float HalfLength{std::sqrt(std::max(GridRadius * GridRadius - Offset * Offset, 0.0f))};
        const float Start{std::max(MinimumX, FadeCenter.mX - HalfLength)};
        const float End{std::min(MaximumX, FadeCenter.mX + HalfLength)};
        if (End > Start) {
            Lines.AddGridLine(FVector3{Start, Position, 0.0f}, FVector3{End, Position, 0.0f}, FVector4{0.5f, 0.5f, 0.5f, 1.0f}, WidthPixels, TierSpacing, DepthMode);
        }
    }
}

void EditorViewport::BuildAxis(FLineRenderData& Lines, ELineDepthMode DepthMode) {
    Lines.AddRay(FVector3{0.0f, 0.0f, 0.0f}, FVector3{1.0f, 0.0f, 0.0f}, 1000.0f, FVector4{1.0f, 0.0f, 0.0f, 1.0f}, 3.0f, DepthMode);
    Lines.AddRay(FVector3{0.0f, 0.0f, 0.0f}, FVector3{-1.0f, 0.0f, 0.0f}, 1000.0f, FVector4{1.0f, 0.0f, 0.0f, 1.0f}, 3.0f, DepthMode);

    Lines.AddRay(FVector3{0.0f, 0.0f, 0.0f}, FVector3{0.0f, 1.0f, 0.0f}, 1000.0f, FVector4{0.0f, 1.0f, 0.0f, 1.0f}, 3.0f, DepthMode);
    Lines.AddRay(FVector3{0.0f, 0.0f, 0.0f}, FVector3{0.0f, -1.0f, 0.0f}, 1000.0f, FVector4{0.0f, 1.0f, 0.0f, 1.0f}, 3.0f, DepthMode);

    Lines.AddRay(FVector3{0.0f, 0.0f, 0.0f}, FVector3{0.0f, 0.0f, 1.0f}, 1000.0f, FVector4{0.0f, 0.0f, 1.0f, 1.0f}, 3.0f, DepthMode);
    Lines.AddRay(FVector3{0.0f, 0.0f, 0.0f}, FVector3{0.0f, 0.0f, -1.0f}, 1000.0f, FVector4{0.0f, 0.0f, 1.0f, 1.0f}, 3.0f, DepthMode);
}

void EditorViewport::BuildBounds(FLineRenderData& Lines, const CameraProbe& Camera, ELineDepthMode DepthMode) {
    if (mEditorContext == nullptr)
        return;

    const UActorComponent* SelectedComponent{mEditorContext->GetSelectedComponent()};
    if (SelectedComponent == nullptr)
        return;

    if (SelectedComponent->GetTypeInfo()->IsA<UCollisionComponent>()) {
        const auto* CollisionComponent{static_cast<const UCollisionComponent*>(SelectedComponent)};
        CollisionComponent->DrawEditorBounds(&Lines, DepthMode);
    } else if (SelectedComponent->GetTypeInfo()->IsA<UBillboardComponent>()) {
        FMatrix CameraWorld{};
        if (!Camera.mView.TryInverse(CameraWorld))
            return;
        const UBillboardComponent* Billboard{static_cast<const UBillboardComponent*>(SelectedComponent)};
        std::array<FVector3, 4> Corners{};
        if (!Billboard->GetWorldCorners(CameraWorld, Corners))
            return;

        const FVector4 LineColor{0.0f, 0.0f, 1.0f, 1.0f};
        Lines.AddLine(Corners[0], Corners[1], LineColor, 1.0f, DepthMode);
        Lines.AddLine(Corners[1], Corners[3], LineColor, 1.0f, DepthMode);
        Lines.AddLine(Corners[3], Corners[2], LineColor, 1.0f, DepthMode);
        Lines.AddLine(Corners[2], Corners[0], LineColor, 1.0f, DepthMode);

        FVector3 Minimum{Corners[0]};
        FVector3 Maximum{Corners[0]};
        for (const FVector3& Corner : Corners) {
            Minimum = FVector3::Min(Minimum, Corner);
            Maximum = FVector3::Max(Maximum, Corner);
        }
        const std::array<FVector3, 8> BoxCorners{FVector3{Minimum.mX, Minimum.mY, Minimum.mZ}, FVector3{Maximum.mX, Minimum.mY, Minimum.mZ}, FVector3{Maximum.mX, Maximum.mY, Minimum.mZ}, FVector3{Minimum.mX, Maximum.mY, Minimum.mZ}, FVector3{Minimum.mX, Minimum.mY, Maximum.mZ}, FVector3{Maximum.mX, Minimum.mY, Maximum.mZ}, FVector3{Maximum.mX, Maximum.mY, Maximum.mZ}, FVector3{Minimum.mX, Maximum.mY, Maximum.mZ}};
        const FVector4 BoxColor{1.0f, 0.0f, 0.0f, 1.0f};
        constexpr std::array<std::array<std::size_t, 2>, 12> BoxEdges{std::array<std::size_t, 2>{0, 1}, std::array<std::size_t, 2>{1, 2}, std::array<std::size_t, 2>{2, 3}, std::array<std::size_t, 2>{3, 0}, std::array<std::size_t, 2>{4, 5}, std::array<std::size_t, 2>{5, 6}, std::array<std::size_t, 2>{6, 7}, std::array<std::size_t, 2>{7, 4}, std::array<std::size_t, 2>{0, 4}, std::array<std::size_t, 2>{1, 5}, std::array<std::size_t, 2>{2, 6}, std::array<std::size_t, 2>{3, 7}};
        for (const std::array<std::size_t, 2>& Edge : BoxEdges) {
            Lines.AddLine(BoxCorners[Edge[0]], BoxCorners[Edge[1]], BoxColor, 1.0f, DepthMode);
        }
    } else if (SelectedComponent->GetTypeInfo()->IsA<UMeshComponent>()) {
        const auto* MeshComponent{static_cast<const UMeshComponent*>(SelectedComponent)};

        auto& BB{MeshComponent->GetPickingBox()};
        DirectX::BoundingOrientedBox WorldBB{};
        BB.Transform(WorldBB, MeshComponent->GetComponentToWorld().ToSimpleMath());

        std::array<DirectX::XMFLOAT3, DirectX::BoundingOrientedBox::CORNER_COUNT> Corners{};
        WorldBB.GetCorners(Corners.data());

        const FVector4 LineColor{FVector4{0.0f, 0.0f, 1.0f, 1.0f}};
        const float Thickness{1.0f};
        const auto AddEdge{[&Lines, &Corners, LineColor, Thickness, DepthMode](std::size_t Start, std::size_t End) {
            Lines.AddLine(FVector3{Corners[Start]}, FVector3{Corners[End]}, LineColor, Thickness, DepthMode);
        }};

        AddEdge(0, 1);
        AddEdge(1, 2);
        AddEdge(2, 3);
        AddEdge(3, 0);
        AddEdge(4, 5);
        AddEdge(5, 6);
        AddEdge(6, 7);
        AddEdge(7, 4);
        AddEdge(0, 4);
        AddEdge(1, 5);
        AddEdge(2, 6);
        AddEdge(3, 7);

        DirectX::XMFLOAT3 Min{Corners[0]};
        DirectX::XMFLOAT3 Max{Corners[0]};

        for (const auto& Corner : Corners) {
            Min.x = std::min(Min.x, Corner.x);
            Min.y = std::min(Min.y, Corner.y);
            Min.z = std::min(Min.z, Corner.z);

            Max.x = std::max(Max.x, Corner.x);
            Max.y = std::max(Max.y, Corner.y);
            Max.z = std::max(Max.z, Corner.z);
        }

        std::array<DirectX::XMFLOAT3, 8> AABBCorners{ DirectX::XMFLOAT3{Min.x, Min.y, Min.z}, DirectX::XMFLOAT3{Max.x, Min.y, Min.z}, DirectX::XMFLOAT3{Max.x, Max.y, Min.z}, DirectX::XMFLOAT3{Min.x, Max.y, Min.z}, DirectX::XMFLOAT3{Min.x, Min.y, Max.z}, DirectX::XMFLOAT3{Max.x, Min.y, Max.z}, DirectX::XMFLOAT3{Max.x, Max.y, Max.z}, DirectX::XMFLOAT3{Min.x, Max.y, Max.z}};

        const FVector4 AABBColor{FVector4{1.0f, 0.0f, 0.0f, 1.0f}};

        const auto AddAABBEdge{[&Lines, &AABBCorners, AABBColor, Thickness, DepthMode](std::size_t Start, std::size_t End) {
            Lines.AddLine(FVector3{AABBCorners[Start]}, FVector3{AABBCorners[End]}, AABBColor, Thickness, DepthMode);
        }};

        AddAABBEdge(0, 1);
        AddAABBEdge(1, 2);
        AddAABBEdge(2, 3);
        AddAABBEdge(3, 0);

        AddAABBEdge(4, 5);
        AddAABBEdge(5, 6);
        AddAABBEdge(6, 7);
        AddAABBEdge(7, 4);

        AddAABBEdge(0, 4);
        AddAABBEdge(1, 5);
        AddAABBEdge(2, 6);
        AddAABBEdge(3, 7);
    }
}

void EditorViewport::BuildViewRenderData(FRenderView& View, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport) {
    mTransformGizmo.Update(Camera, Viewport);
    mTransformGizmo.BuildGizmoProbes(View.mGizmoProbes);
    View.mSceneGuides.Clear();
    const ELineDepthMode DepthMode{ELineDepthMode::DepthTested};
    const FEditorSettings Settings{mEditorContext != nullptr ? mEditorContext->GetEditorSettings() : FEditorSettings{}};
    FVector2D FadeCenter{CameraPosition.mX, CameraPosition.mY};
    if (Settings.mGridVisible) {
        BuildGrid(View.mSceneGuides, Camera, CameraPosition, Viewport, FadeCenter, DepthMode);
    }
    if (Settings.mAxisVisible) {
        BuildAxis(View.mSceneGuides, DepthMode);
    }
    BuildBounds(View.mSceneGuides, Camera, DepthMode);
    View.mGridFade = FVector4{FadeCenter.mX, FadeCenter.mY, 450.0f, 550.0f};
}
