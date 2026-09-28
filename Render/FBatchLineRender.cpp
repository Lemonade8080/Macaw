#include "pch.h"
#include "FBatchLineRender.h"
#include "FFrameResource.h"

void FBatchLineRenderer::Initialize(ID3D11Device* InDevice, Uint32 InitialLineCapacity) {
    ErrorHandler::Report(InDevice == nullptr, "[ FLineRenderer ]", "Invalid device pointer.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(InitialLineCapacity == 0, "[ FLineRenderer ]", "Initial line capacity must be greater than zero.", ErrorHandler::EErrorLevel::Critical);

    Reset();

    mDevice = InDevice;
    mDepthTestedPipeline = std::make_unique<UPipeline>();
    mOverlayPipeline = std::make_unique<UPipeline>();

    ErrorHandler::Report(!mDepthTestedPipeline->Initialize(mDevice, "./Content/Pipeline/BatchLineDepthTested.json"), "[ FBatchLineRenderer ]", "Failed to initialize the depth-tested batch line pipeline.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(!mOverlayPipeline->Initialize(mDevice, "./Content/Pipeline/BatchLineOverlay.json"), "[ FBatchLineRenderer ]", "Failed to initialize the overlay batch line pipeline.", ErrorHandler::EErrorLevel::Critical);

    InitialLineCapacity = std::max(InitialLineCapacity * 2, 2u);


    mDepthTestedBatch.mVertices.reserve(InitialLineCapacity);
    mOverlayBatch.mVertices.reserve(InitialLineCapacity);
}

void FBatchLineRenderer::Reset() {
    mDepthTestedBatch.mVertices.clear();

    mOverlayBatch.mVertices.clear();

    mDepthTestedPipeline = nullptr;
    mOverlayPipeline = nullptr;
    mDevice = nullptr;
}

void FBatchLineRenderer::AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
    if (WidthPixels <= 0.0f || (End - Start).LengthSquared() <= 0.0f) {
        return;
    }

    FLineBatch& Batch{DepthMode == ELineDepthMode::DepthTested ? mDepthTestedBatch : mOverlayBatch};

    Batch.mVertices.emplace_back(FBatchLineInstance{ .mPosition = FVector3{Start.mX, Start.mY, Start.mZ}, .mColor = Color});
    Batch.mVertices.emplace_back(FBatchLineInstance{ .mPosition = FVector3{End.mX, End.mY, End.mZ}, .mColor = Color});
}

void FBatchLineRenderer::AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
    if (Length <= 0.0f || Direction.LengthSquared() <= 0.0f) {
        return;
    }

    FVector3 NormalizedDirection{Direction};
    NormalizedDirection.Normalize();

    AddLine(Origin, Origin + NormalizedDirection * Length, Color, WidthPixels, DepthMode);
}

void FBatchLineRenderer::Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource) {
    ErrorHandler::Report(mDevice == nullptr, "[ FLineRenderer ]", "Invalid device pointer.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(Context == nullptr, "[ FLineRenderer ]", "Invalid device context pointer.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(mDepthTestedPipeline == nullptr, "[ FLineRenderer ]", "Depth-tested pipeline is not initialized.", ErrorHandler::EErrorLevel::Critical);

    ErrorHandler::Report(not FrameResource.BindCommon(Context), "[ FLineRenderer ]", "Failed to bind frame constants.", ErrorHandler::EErrorLevel::Critical);

    ErrorHandler::Report(not RenderBatch(Context, FrameResource, mDepthTestedBatch, mDepthTestedPipeline.get(), EFrameStream::BatchLineDepth), "[ FLineRenderer ]", "Failed to render depth-tested lines.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(not RenderBatch(Context, FrameResource, mOverlayBatch, mOverlayPipeline.get(), EFrameStream::BatchLineOverlay), "[ FLineRenderer ]", "Failed to render overlay lines.", ErrorHandler::EErrorLevel::Critical);

    Clear();
}

void FBatchLineRenderer::Clear() {
    mDepthTestedBatch.mVertices.clear();
    mOverlayBatch.mVertices.clear();
}

Uint32 FBatchLineRenderer::GetLineCount() const {
    return static_cast<Uint32>(mDepthTestedBatch.mVertices.size() + mOverlayBatch.mVertices.size());
}

bool FBatchLineRenderer::IsEmpty() const {
    return mDepthTestedBatch.mVertices.empty() && mOverlayBatch.mVertices.empty();
}

bool FBatchLineRenderer::RenderBatch(ID3D11DeviceContext* Context, FFrameResource& FrameResource, FLineBatch& Batch, const UPipeline* Pipeline, EFrameStream Stream) {
    if (Batch.mVertices.empty()) {
        return true;
    }

    if (mDevice == nullptr || Context == nullptr || Pipeline == nullptr) {
        return false;
    }

    const Uint32 VertexCount{static_cast<Uint32>(Batch.mVertices.size())};
    if (!FrameResource.UploadStream(mDevice, Context, Stream, Batch.mVertices.data(), VertexCount, sizeof(FBatchLineInstance), D3D11_BIND_VERTEX_BUFFER)) {
        return false;
    }

    Pipeline->Bind(Context);

    ID3D11Buffer* VertexBuffers[]{FrameResource.GetStreamBuffer(Stream)};
    const Uint32 Strides[]{static_cast<Uint32>(sizeof(FBatchLineInstance))};
    const Uint32 Offsets[]{0};

    Context->IASetVertexBuffers(0, 1, VertexBuffers, Strides, Offsets);

    Context->Draw(VertexCount, 0);

    return true;
}
