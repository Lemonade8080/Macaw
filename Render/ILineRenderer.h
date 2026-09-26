#pragma once

#include <d3d11.h>

#include "Core/Render/ILineDrawContext.h"

#include "Render/Buffer/FGraphicsBuffer.h"
#include "Render/Buffer/TGraphicsRootConstants.h"
#include "Asset/Pipeline/UPipeline.h"

struct FLineViewData {
    FMatrix mViewProjection{};
    FVector2D mViewportSize{};
    FVector4 mGridFade{};
};

class ILineRenderer : public ILineDrawContext {
public:
    ILineRenderer() = default;

    ~ILineRenderer() override = default;

    ILineRenderer(const ILineRenderer&) = delete;
    ILineRenderer& operator=(const ILineRenderer&) = delete;

    ILineRenderer(ILineRenderer&&) noexcept = default;
    ILineRenderer& operator=(ILineRenderer&&) noexcept = default;

public:
    virtual void Initialize(ID3D11Device* Device, Uint32 InitialLineCapacity = 1024) = 0;
    virtual void Reset() = 0;

    virtual void AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested) = 0;

    virtual void Render(ID3D11DeviceContext* Context, const FLineViewData& ViewData) = 0;
    virtual void Clear() = 0;

    [[nodiscard]] virtual Uint32 GetLineCount() const = 0;
    [[nodiscard]] virtual bool IsEmpty() const = 0;
};
