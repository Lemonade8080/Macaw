#pragma once

#include "ILineRenderer.h"

class FBatchLineRenderer : public ILineRenderer {
private:
    struct FBatchLineInstance {
        FVector3 mPosition{};
        FVector4 mColor{};
    };

    struct FLineBatch {
        TArray<FBatchLineInstance> mVertices{};
    };

public:
    FBatchLineRenderer() = default;
    ~FBatchLineRenderer() = default;

    FBatchLineRenderer(const FBatchLineRenderer&) = delete;
    FBatchLineRenderer& operator=(const FBatchLineRenderer&) = delete;

    FBatchLineRenderer(FBatchLineRenderer&&) noexcept = default;
    FBatchLineRenderer& operator=(FBatchLineRenderer&&) noexcept = default;

public:
    void Initialize(ID3D11Device* InDevice, Uint32 InitialLineCapacity = 1024);
    void Reset();

    void AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested);
    void AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested);

    void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource);
    void Clear();

    [[nodiscard]] Uint32 GetLineCount() const;
    [[nodiscard]] bool IsEmpty() const;

private:
    bool RenderBatch(ID3D11DeviceContext* Context, FFrameResource& FrameResource, FLineBatch& Batch, const UPipeline* Pipeline, EFrameStream Stream);

private:
    ID3D11Device* mDevice{nullptr};

    std::unique_ptr<UPipeline> mDepthTestedPipeline{};
    std::unique_ptr<UPipeline> mOverlayPipeline{};

    FLineBatch mDepthTestedBatch{};
    FLineBatch mOverlayBatch{};

};
