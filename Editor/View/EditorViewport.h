#pragma once

#include <d3d11.h>

#include "Asset/FAssetRegistry.h"
#include "Render/FRenderView.h"
#include "../../Core/Channel/FStateChannel.h"
#include "World/FWorldEditorContext.h"
#include "Editor/Input/FMouseInput.h"

#include "Core/Render/FLineRenderData.h"
#include "FTransformGizmo.h"

class EditorViewport {
public:
    EditorViewport() = default;
    ~EditorViewport() = default;

    EditorViewport(const EditorViewport&) = delete;
    EditorViewport& operator=(const EditorViewport&) = delete;

    EditorViewport(EditorViewport&&) noexcept = default;
    EditorViewport& operator=(EditorViewport&&) noexcept = default;

public:
    void Initialize(ID3D11Device* Device, FAssetRegistry& AssetRegistry, FWorldEditorContext& InEditorContext);

    void PrepareInput(const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport);
    void ProcessInput(FKeyboardInput& KeyboardInput, FMouseInput& MouseInput, bool BMouseCapturedByUi);
    void BuildViewRenderData(FRenderView& View, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport);

    FStateChannel<Uint8>::FReadWriter GetGizmoMode();
    FStateChannel<Uint8>::FReadWriter GetGizmoCoordinateSpace();

private:
    void BuildGrid(FLineRenderData& Lines, const CameraProbe& Camera, const FVector3& CameraPosition, const D3D11_VIEWPORT& Viewport, FVector2D& FadeCenter, ELineDepthMode DepthMode);
    void BuildAxis(FLineRenderData& Lines, ELineDepthMode DepthMode);
    void BuildBounds(FLineRenderData& Lines, const CameraProbe& Camera, ELineDepthMode DepthMode);

private:
    FTransformGizmo mTransformGizmo{};

    FWorldEditorContext* mEditorContext{nullptr};
};
