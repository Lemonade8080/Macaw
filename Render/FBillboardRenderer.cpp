#include "pch.h"
#include "FBillboardRenderer.h"

#include "Asset/Pipeline/UPipeline.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UTexture.h"
#include "FFrameResource.h"
#include <algorithm>
#include <unordered_map>

bool FBillboardRenderer::Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity) {
    if (InDevice == nullptr || InitialCapacity == 0) {
        return false;
    }
    mDevice = InDevice;

    return true;
}

void FBillboardRenderer::Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource, const TArray<FBillboardProbe>& BillboardProbe, const IAssetRegistry* AssetRegistry, ERenderMode Mode) {
    if (Context == nullptr || mDevice == nullptr || AssetRegistry == nullptr || BillboardProbe.empty() || !FrameResource.HasCameraWorld() || !FrameResource.BindCommon(Context)) {
        return;
    }

    // Release Vertex buffer, Index buffer
    UINT Stride{0};
    UINT Offset{0};
    ID3D11Buffer* NullBuffer{nullptr};
    Context->IASetVertexBuffers(0, 1, &NullBuffer, &Stride, &Offset);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

    // Pipeline, Texture Batch
    struct FBatchKey {
        FAssetHandle mPipelineHandle{};
        FAssetHandle mTextureHandle{};
        bool operator==(const FBatchKey& Other) const = default;
    };

    struct FBatchKeyHash {
        std::size_t operator()(const FBatchKey& Key) const noexcept {
            return std::hash<std::uint32_t>{}(Key.mPipelineHandle.mId) ^ (std::hash<std::uint32_t>{}(Key.mTextureHandle.mId) << 1);
        }
    };

    std::unordered_map<FBatchKey, TArray<FBillboardData>, FBatchKeyHash> Batches{};
    for (const FBillboardProbe& Probe : BillboardProbe) {
        if (!Probe.mPipelineHandle || !Probe.mTextureHandle) {
            continue;
        }
        Batches[{Probe.mPipelineHandle, Probe.mTextureHandle}].push_back(FBillboardData{ .mWorld = Probe.mWorld, .mSize = Probe.mSize, .mUvMin = Probe.mUvMin, .mUvMax = Probe.mUvMax, .mPad = FVector2{0.0f, 0.0f}, .mColor = Probe.mColor});
    }

    for (auto& [Key, InstanceArray] : Batches) {
        const UPipeline* Pipeline{AssetRegistry->ResolveAsset<UPipeline>(Key.mPipelineHandle)};
        const UTexture* Texture{AssetRegistry->ResolveAsset<UTexture>(Key.mTextureHandle)};
        if (Pipeline == nullptr || Texture == nullptr || InstanceArray.empty()) {
            continue;
        }
        const std::uint32_t InstanceCount{static_cast<std::uint32_t>(InstanceArray.size())};
        if (!FrameResource.UploadStream(mDevice, Context, EFrameStream::Billboard, InstanceArray.data(), InstanceCount, sizeof(FBillboardData), D3D11_BIND_SHADER_RESOURCE)) {
            continue;
        }

        Pipeline->Bind(Context, Pipeline->ResolveRenderMode(Mode));

        ID3D11ShaderResourceView* BufferSRV{FrameResource.GetStreamResourceView(EFrameStream::Billboard)};
        Context->GSSetShaderResources(0, 1, &BufferSRV);
        ID3D11ShaderResourceView* TextureSRV{Texture->GetSRV()};
        Context->PSSetShaderResources(3, 1, &TextureSRV);
        Context->DrawInstanced(1, InstanceCount, 0, 0);
    }
}
