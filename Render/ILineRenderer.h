#pragma once

#include <d3d11.h>

#include "Core/Render/ILineDrawContext.h"

#include "Render/Buffer/FGraphicsBuffer.h"
#include "Asset/Pipeline/UPipeline.h"

class FFrameResource;
enum class EFrameStream : Uint8;

class ILineRenderer : public ILineDrawContext {
public:
    ILineRenderer() = default;

    // unique_ptr<ILineRenderer> 로 파생 객체를 들고 있으므로 반드시 virtual 이어야 한다.
    // 아니면 파생 소멸자가 불리지 않아 D3D 리소스가 누수되고 종료 시 크래시한다.
    ~ILineRenderer() override = default;

    ILineRenderer(const ILineRenderer&) = delete;
    ILineRenderer& operator=(const ILineRenderer&) = delete;

    ILineRenderer(ILineRenderer&&) noexcept = default;
    ILineRenderer& operator=(ILineRenderer&&) noexcept = default;

public:
    virtual void Initialize(ID3D11Device* Device, Uint32 InitialLineCapacity = 1024) = 0;
    virtual void Reset() = 0;

    virtual void AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested) = 0;

    virtual void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource) = 0;
    virtual void Clear() = 0;

    [[nodiscard]] virtual Uint32 GetLineCount() const = 0;
    [[nodiscard]] virtual bool IsEmpty() const = 0;
};
