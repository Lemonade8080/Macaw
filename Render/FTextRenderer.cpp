#include "pch.h"
#include "FTextRenderer.h"

#include "Asset/Pipeline/UPipeline.h"

#include "Asset/IRenderAssetRegistry.h"
#include "Asset/UFont.h"

#include <algorithm>
#include <limits>

bool FTextRenderer::Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity) {
    if (InDevice == nullptr || InitialCapacity == 0) {
        return false;
    }
    mDevice = InDevice;
    if (!mTextConstants.Initialize(mDevice)) {
        return false;
    }
    return EnsureCapacity(InitialCapacity);
}

bool FTextRenderer::EnsureCapacity(std::uint32_t RequiredCapacity) {
    if (RequiredCapacity <= mVertexCapacity) {
        return true;
    }
    std::uint32_t NewCapacity{std::max(mVertexCapacity, 1u)};
    while (NewCapacity < RequiredCapacity) {
        NewCapacity *= 2;
    }
    if (NewCapacity > std::numeric_limits<std::uint32_t>::max() / sizeof(FTextVertex)) {
        return false;
    }
    FGraphicsBufferDescription Description{};
    Description.mByteSize = NewCapacity * sizeof(FTextVertex);
    Description.mUsage = D3D11_USAGE_DYNAMIC;
    Description.mBindFlags = D3D11_BIND_VERTEX_BUFFER;
    Description.mCpuAccessFlags = D3D11_CPU_ACCESS_WRITE;

    FGraphicsBuffer NewBuffer{};
    if (!NewBuffer.Initialize(mDevice, Description)) {
        return false;
    }
    mVertexBuffer = std::move(NewBuffer);
    mVertexCapacity = NewCapacity;
    return true;
}

void FTextRenderer::Render(ID3D11DeviceContext* Context, const TArray<FTextProbe>& TextProbes, const CameraProbe& Camera, IRenderAssetRegistry* AssetRegistry) {
    if (Context == nullptr || mDevice == nullptr || AssetRegistry == nullptr || TextProbes.empty()) {
        return;
    }
    FMatrix CameraWorld{};
    if (!Camera.mView.TryInverse(CameraWorld)) {
        return;
    }
    for (const FTextProbe& Probe : TextProbes) {
        if (Probe.mVertices.empty()) {
            continue;
        }
        const UFont* Font{AssetRegistry->ResolveAsset<UFont>(Probe.mFontHandle)};
        if (Font == nullptr) {
            continue;
        }
        AssetRegistry->FlushFontAtlas(Probe.mFontHandle, Context);
        ID3D11ShaderResourceView* AtlasSRV{Font->GetAtlasSRV()};
        if (AtlasSRV == nullptr) {
            if (AtlasSRV == nullptr) {
                continue;
            }
        }
        const UPipeline* PipeLine{AssetRegistry->ResolveAsset<UPipeline>(Probe.mPipelineHandle)};
        if (PipeLine == nullptr) {
            continue;
        }
        const std::uint32_t VertexCount{static_cast<std::uint32_t>(Probe.mVertices.size())};
        const std::uint32_t VertexByteSize{VertexCount * sizeof(FTextVertex)};
        if (!EnsureCapacity(VertexCount)) {
            continue;
        }
        if (!mVertexBuffer.WriteDiscard(Context, Probe.mVertices.data(), VertexByteSize)) {
            continue;
        }
        PipeLine->Bind(Context);
        ID3D11Buffer* Buffer{mVertexBuffer.GetBuffer()};
        UINT Stride{sizeof(FTextVertex)};
        UINT Offset{0};
        Context->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
        Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        Context->PSSetShaderResources(3, 1, &AtlasSRV);
        FTextConstants Constants{ .mWorld = Probe.mWorld, .mViewwProjection = Camera.mViewProjection, .mCameraWorld = CameraWorld, .mColor = Probe.mColor, .mScreenBoundsExtent = Probe.mScreenBoundsExtent, .mScreenUpPadding = Probe.mScreenUpPadding};
        if (!mTextConstants.SetGraphicsRoot32BitConstants(Constants, 0)) {
            continue;
        }
        if (!mTextConstants.Bind(Context, 0, EGraphicsShaderStage::Geometry | EGraphicsShaderStage::Pixel)) {
            continue;
        }
        Context->Draw(VertexCount, 0);
    }
}
