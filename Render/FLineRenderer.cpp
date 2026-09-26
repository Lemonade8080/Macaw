#include "pch.h"

#include "FLineRenderer.h"

#include "Core/Base/ErrorHandler.h"

#include <algorithm>
#include <array>
#include <limits>

void FLineRenderer::Initialize(ID3D11Device* InDevice, Uint32 InitialLineCapacity) {
    ErrorHandler::Report(InDevice == nullptr, "[ FLineRenderer ]", "Invalid device pointer.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(InitialLineCapacity == 0, "[ FLineRenderer ]", "Initial line capacity must be greater than zero.", ErrorHandler::EErrorLevel::Critical);

    Reset();

    mDevice = InDevice;
    mDepthTestedPipeline = std::make_unique<UPipeline>();
    mOverlayPipeline = std::make_unique<UPipeline>();

    ErrorHandler::Report(!mDepthTestedPipeline->Initialize(mDevice, "./Content/Pipeline/LineDepthTested.json"), "[ FLineRenderer ]", "Failed to initialize the depth-tested line pipeline.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(!mOverlayPipeline->Initialize(mDevice, "./Content/Pipeline/LineOverlay.json"), "[ FLineRenderer ]", "Failed to initialize the overlay line pipeline.", ErrorHandler::EErrorLevel::Critical);

    InitialLineCapacity = std::max(InitialLineCapacity, 1u);

    ErrorHandler::Report(not CreateQuadGeometry(mDevice), "[ FLineRenderer ]", "Failed to create quad geometry.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(not CreateInstanceBuffer(mDevice, mDepthTestedBatch, InitialLineCapacity), "[ FLineRenderer ]", "Failed to create instance buffer for depth-tested lines.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(not CreateInstanceBuffer(mDevice, mOverlayBatch, InitialLineCapacity), "[ FLineRenderer ]", "Failed to create instance buffer for overlay lines.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(not mFrameConstants.Initialize(mDevice), "[ FLineRenderer ]", "Failed to initialize frame constants.", ErrorHandler::EErrorLevel::Critical);

    mDepthTestedBatch.mInstances.reserve(InitialLineCapacity);
    mOverlayBatch.mInstances.reserve(InitialLineCapacity);
}

void FLineRenderer::Reset() {
    mQuadVertexBuffer.Reset();
    mQuadIndexBuffer.Reset();

    mDepthTestedBatch.mInstanceBuffer.Reset();
    mDepthTestedBatch.mInstances.clear();
    mDepthTestedBatch.mCapacity = 0;

    mOverlayBatch.mInstanceBuffer.Reset();
    mOverlayBatch.mInstances.clear();
    mOverlayBatch.mCapacity = 0;

    mFrameConstants.Reset();

    mDepthTestedPipeline = nullptr;
    mOverlayPipeline = nullptr;
    mDevice = nullptr;
}

void FLineRenderer::AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
    AddLineInternal(Start, End, Color, WidthPixels, DepthMode, 0.0f);
}

void FLineRenderer::AddGridLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, float GridSpacing, ELineDepthMode DepthMode) {
    AddLineInternal(Start, End, Color, WidthPixels, DepthMode, GridSpacing);
}

void FLineRenderer::AddLineInternal(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode, float GridSpacing) {
    if (WidthPixels <= 0.0f || (End - Start).LengthSquared() <= 0.0f) {
        return;
    }

    FLineBatch& Batch{DepthMode == ELineDepthMode::DepthTested ? mDepthTestedBatch : mOverlayBatch};

    Batch.mInstances.emplace_back(FLineInstance{ .mStartAndWidth = FVector4{Start.mX, Start.mY, Start.mZ, WidthPixels}, .mEndAndPadding = FVector4{End.mX, End.mY, End.mZ, GridSpacing}, .mColor = Color});
}

void FLineRenderer::AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
    if (Length <= 0.0f || Direction.LengthSquared() <= 0.0f) {
        return;
    }

    FVector3 NormalizedDirection{Direction};
    NormalizedDirection.Normalize();

    AddLine(Origin, Origin + NormalizedDirection * Length, Color, WidthPixels, DepthMode);
}

void FLineRenderer::Render(ID3D11DeviceContext* Context, const FLineViewData& ViewData) {
    ErrorHandler::Report(mDevice == nullptr, "[ FLineRenderer ]", "Invalid device pointer.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(Context == nullptr, "[ FLineRenderer ]", "Invalid device context pointer.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(ViewData.mViewportSize.mX <= 0.0f || ViewData.mViewportSize.mY <= 0.0f, "[ FLineRenderer ]", "Invalid viewport size.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(mDepthTestedPipeline == nullptr, "[ FLineRenderer ]", "Depth-tested pipeline is not initialized.", ErrorHandler::EErrorLevel::Critical);

    const FLineFrameConstants Constants{ .mViewProjection = ViewData.mViewProjection, .mViewport = FVector4{ ViewData.mViewportSize.mX, ViewData.mViewportSize.mY, 1.0f / ViewData.mViewportSize.mX, 1.0f / ViewData.mViewportSize.mY}, .mGridFade = ViewData.mGridFade};

    ErrorHandler::Report(not mFrameConstants.SetGraphicsRoot32BitConstants(Constants), "[ FLineRenderer ]", "Failed to set frame constants.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(not mFrameConstants.Bind(Context, 0, EGraphicsShaderStage::Vertex | EGraphicsShaderStage::Pixel), "[ FLineRenderer ]", "Failed to bind frame constants.", ErrorHandler::EErrorLevel::Critical);

    ErrorHandler::Report(not RenderBatch(mDevice, Context, mDepthTestedBatch, mDepthTestedPipeline.get()), "[ FLineRenderer ]", "Failed to render depth-tested lines.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::Report(not RenderBatch(mDevice, Context, mOverlayBatch, mOverlayPipeline.get()), "[ FLineRenderer ]", "Failed to render overlay lines.", ErrorHandler::EErrorLevel::Critical);

    Clear();
}

void FLineRenderer::Clear() {
    mDepthTestedBatch.mInstances.clear();
    mOverlayBatch.mInstances.clear();
}

Uint32 FLineRenderer::GetLineCount() const {
    return static_cast<Uint32>(mDepthTestedBatch.mInstances.size() + mOverlayBatch.mInstances.size());
}

bool FLineRenderer::IsEmpty() const {
    return mDepthTestedBatch.mInstances.empty() && mOverlayBatch.mInstances.empty();
}

bool FLineRenderer::CreateQuadGeometry(ID3D11Device* InDevice) {
    const std::array<FQuadVertex, 4> Vertices{ FQuadVertex{FVector2D{0.0f, -1.0f}}, FQuadVertex{FVector2D{0.0f, 1.0f}}, FQuadVertex{FVector2D{1.0f, -1.0f}}, FQuadVertex{FVector2D{1.0f, 1.0f}}};

    constexpr std::array<Uint16, 6> Indices{0, 1, 2, 2, 1, 3};

    FGraphicsBufferDescription VertexBufferDescription{};
    VertexBufferDescription.mByteSize = static_cast<Uint32>(sizeof(Vertices));
    VertexBufferDescription.mUsage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDescription.mBindFlags = D3D11_BIND_VERTEX_BUFFER;

    if (!mQuadVertexBuffer.Initialize(InDevice, VertexBufferDescription, Vertices.data())) {
        return false;
    }

    FGraphicsBufferDescription IndexBufferDescription{};
    IndexBufferDescription.mByteSize = static_cast<Uint32>(sizeof(Indices));
    IndexBufferDescription.mUsage = D3D11_USAGE_IMMUTABLE;
    IndexBufferDescription.mBindFlags = D3D11_BIND_INDEX_BUFFER;

    return mQuadIndexBuffer.Initialize(InDevice, IndexBufferDescription, Indices.data());
}

bool FLineRenderer::CreateInstanceBuffer(ID3D11Device* InDevice, FLineBatch& Batch, Uint32 Capacity) {
    if (InDevice == nullptr || Capacity == 0 || Capacity > std::numeric_limits<Uint32>::max() / sizeof(FLineInstance)) {
        return false;
    }

    FGraphicsBufferDescription Description{};
    Description.mByteSize = static_cast<Uint32>(Capacity * sizeof(FLineInstance));
    Description.mUsage = D3D11_USAGE_DYNAMIC;
    Description.mBindFlags = D3D11_BIND_VERTEX_BUFFER;
    Description.mCpuAccessFlags = D3D11_CPU_ACCESS_WRITE;

    FGraphicsBuffer NewBuffer{};

    if (!NewBuffer.Initialize(InDevice, Description)) {
        return false;
    }

    Batch.mInstanceBuffer = std::move(NewBuffer);
    Batch.mCapacity = Capacity;

    return true;
}

bool FLineRenderer::EnsureCapacity(ID3D11Device* InDevice, FLineBatch& Batch, Uint32 RequiredCapacity) {
    if (RequiredCapacity <= Batch.mCapacity) {
        return true;
    }

    Uint32 NewCapacity{std::max(Batch.mCapacity, 1u)};

    while (NewCapacity < RequiredCapacity) {
        if (NewCapacity > std::numeric_limits<Uint32>::max() / 2) {
            NewCapacity = RequiredCapacity;
            break;
        }

        NewCapacity *= 2;
    }

    return CreateInstanceBuffer(InDevice, Batch, NewCapacity);
}

bool FLineRenderer::RenderBatch(ID3D11Device* InDevice, ID3D11DeviceContext* Context, FLineBatch& Batch, const UPipeline* Pipeline) {
    if (Batch.mInstances.empty()) {
        return true;
    }

    if (InDevice == nullptr or Context == nullptr or Pipeline == nullptr) {
        return false;
    }

    const Uint32 InstanceCount{static_cast<Uint32>(Batch.mInstances.size())};
    const Uint32 InstanceByteSize{static_cast<Uint32>(InstanceCount * sizeof(FLineInstance))};

    if (not EnsureCapacity(InDevice, Batch, InstanceCount) or not Batch.mInstanceBuffer.WriteDiscard(Context, Batch.mInstances.data(), InstanceByteSize)) {
        return false;
    }

    Pipeline->Bind(Context);

    ID3D11Buffer* VertexBuffers[]{ mQuadVertexBuffer.GetBuffer(), Batch.mInstanceBuffer.GetBuffer()};

    const Uint32 Strides[]{ static_cast<Uint32>(sizeof(FQuadVertex)), static_cast<Uint32>(sizeof(FLineInstance))};

    constexpr Uint32 Offsets[]{0, 0};

    Context->IASetVertexBuffers(0, _countof(VertexBuffers), VertexBuffers, Strides, Offsets);
    Context->IASetIndexBuffer(mQuadIndexBuffer.GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
    Context->DrawIndexedInstanced(6, InstanceCount, 0, 0, 0);

    return true;
}
