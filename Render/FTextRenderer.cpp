#include "pch.h"
#include "FTextRenderer.h"

#include "Asset/Pipeline/UPipeline.h"

#include "Asset/IRenderAssetRegistry.h"
#include "Asset/UFont.h"
#include "FFrameResource.h"

#include <algorithm>
#include <limits>

bool FTextRenderer::Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity) {
    if (InDevice == nullptr || InitialCapacity == 0) {
        return false;
    }
    mDevice = InDevice;
    return true;
}

void FTextRenderer::Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource, const TArray<FTextProbe>& TextProbes, IRenderAssetRegistry* AssetRegistry) {
    if (Context == nullptr || mDevice == nullptr || AssetRegistry == nullptr || TextProbes.empty() || !FrameResource.HasCameraWorld() || !FrameResource.BindCommon(Context)) {
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
        if (!FrameResource.UploadStream(mDevice, Context, EFrameStream::Text, Probe.mVertices.data(), VertexCount, sizeof(FTextVertex), D3D11_BIND_VERTEX_BUFFER)) {
            continue;
        }
        PipeLine->Bind(Context);
        ID3D11Buffer* Buffer{FrameResource.GetStreamBuffer(EFrameStream::Text)};
        UINT Stride{sizeof(FTextVertex)};
        UINT Offset{0};
        Context->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
        Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        Context->PSSetShaderResources(3, 1, &AtlasSRV);
        if (!FrameResource.BindTextDraw(Context, Probe)) {
            continue;
        }
        Context->Draw(VertexCount, 0);
    }
}
