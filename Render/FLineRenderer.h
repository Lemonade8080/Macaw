#pragma once

#include "ILineRenderer.h"

class FLineRenderer : public ILineRenderer {
private:
    struct FQuadVertex {
        FVector2D mCorner{};
    };

    struct FLineInstance {
        FVector4 mStartAndWidth{};
        FVector4 mEndAndPadding{};
        FVector4 mColor{};
    };

    struct FLineBatch {
        TArray<FLineInstance> mInstances{};
    };

public:
    FLineRenderer() = default;
    ~FLineRenderer() = default;

    FLineRenderer(const FLineRenderer&) = delete;
    FLineRenderer& operator=(const FLineRenderer&) = delete;

    FLineRenderer(FLineRenderer&&) noexcept = default;
    FLineRenderer& operator=(FLineRenderer&&) noexcept = default;

public:
    void Initialize(ID3D11Device* InDevice, Uint32 InitialLineCapacity = 1024);
    void Reset();

    void AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested);
    void AddGridLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, float GridSpacing, ELineDepthMode DepthMode);
    void AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested);

    void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource);
    void RenderOrientationAxis(ID3D11DeviceContext* Context, FFrameResource& FrameResource);
    void Clear();

    [[nodiscard]] Uint32 GetLineCount() const;
    [[nodiscard]] bool IsEmpty() const;

private:
    void RenderLines(ID3D11DeviceContext* Context, FFrameResource& FrameResource, bool OrientationAxis);
    void AddLineInternal(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode, float GridSpacing);
    bool CreateQuadGeometry(ID3D11Device* Device);
    bool RenderBatch(ID3D11DeviceContext* Context, FFrameResource& FrameResource, FLineBatch& Batch, const UPipeline* Pipeline, EFrameStream Stream);

private:
    ID3D11Device* mDevice{nullptr};

    std::unique_ptr<UPipeline> mDepthTestedPipeline{};
    std::unique_ptr<UPipeline> mOverlayPipeline{};

    FGraphicsBuffer mQuadVertexBuffer{};
    FGraphicsBuffer mQuadIndexBuffer{};

    FLineBatch mDepthTestedBatch{};
    FLineBatch mOverlayBatch{};

};
