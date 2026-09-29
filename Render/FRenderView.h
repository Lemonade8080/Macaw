#pragma once

#include <bitset>
#include "Core/Base/FRenderProbe.h"
#include "Core/Render/FLineRenderData.h"
#include "Asset/Pipeline/UPipeline.h"
#include "IRenderSurface.h"

enum class ERenderPass : Uint8 {
    SceneGeometry,
    SelectionOutline,
    SceneGuides,
    Gizmo,
    Text,
    Billboard,
    OrientationAxis,
    Count
};

struct FRenderView {
    bool IsPassEnabled(ERenderPass Pass) const;
    void SetPassEnabled(ERenderPass Pass, bool Enabled);
    IRenderSurface* mTarget{nullptr};
    CameraProbe mCamera{};
    FRenderSettings mSettings{};
    ERenderMode mRenderMode{ERenderMode::Lit};
    std::bitset<static_cast<std::size_t>(ERenderPass::Count)> mPasses{0x7f};
    float mOrientationAxisSize{};
    FObjectHandle mSelectedActorHandle{};
    TArray<FActorProbe> mGizmoProbes{};
    FLineRenderData mSceneGuides{};
    FVector4 mGridFade{};
};
