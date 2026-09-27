#include "pch.h"
#include "FBillboardRenderer.h"

#include "Asset/Pipeline/UPipeline.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UTexture.h"
#include <algorithm>
#include <unordered_map>

bool FBillboardRenderer::Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity) {
    if (InDevice == nullptr || InitialCapacity == 0) {
        return false;
    }
    mDevice = InDevice;

    if (!mViewConstantBuffer.Initialize(mDevice)) {
        return false;
    }

    return EnsureCapacity(InitialCapacity);
}

void FBillboardRenderer::Render(ID3D11DeviceContext* Context, const TArray<FBillboardProbe>& BillboardProbe, const CameraProbe& Camera, const IAssetRegistry* AssetRegistry, ERenderMode Mode) {
    if (Context == nullptr || mDevice == nullptr || AssetRegistry == nullptr || BillboardProbe.empty()) {
        return;
    }

    FMatrix CameraWorld{};
    if (!Camera.mView.TryInverse(CameraWorld)) {
        return;
    }

    FBillboardViewConstans Constants{ .mViewProjection = Camera.mViewProjection, .mCameraWorld = CameraWorld};

    if (!mViewConstantBuffer.SetGraphicsRoot32BitConstants(Constants, 0)) {
        return;
    }
    mViewConstantBuffer.Bind(Context, 0, EGraphicsShaderStage::Geometry);

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
        if (!EnsureCapacity(InstanceCount)) {
            continue;
        }

        const std::uint32_t ByteSize{InstanceCount * sizeof(FBillboardData)};
        if (!mInstanceBuffer.WriteDiscard(Context, InstanceArray.data(), ByteSize)) {
            continue;
        }

        Pipeline->Bind(Context, Pipeline->ResolveRenderMode(Mode));

        ID3D11ShaderResourceView* BufferSRV{mInstanceBufferSrv.Get()};
        Context->GSSetShaderResources(0, 1, &BufferSRV);
        ID3D11ShaderResourceView* TextureSRV{Texture->GetSRV()};
        Context->PSSetShaderResources(3, 1, &TextureSRV);
        Context->DrawInstanced(1, InstanceCount, 0, 0);
    }
}

bool FBillboardRenderer::EnsureCapacity(Uint32 RequiredCapacity) {
    if (RequiredCapacity <= mInstanceCapacity) {
        return true;
    }

    Uint32 NewCapacity{std::max(mInstanceCapacity, 1u)};

    while (NewCapacity < RequiredCapacity) {
        NewCapacity *= 2;
    }

    FGraphicsBufferDescription Description{};
    Description.mByteSize = NewCapacity * sizeof(FBillboardData);
    Description.mStride = sizeof(FBillboardData);
    Description.mBindFlags = D3D11_BIND_SHADER_RESOURCE;
    Description.mCpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Description.mMiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    Description.mUsage = D3D11_USAGE_DYNAMIC;

    FGraphicsBuffer NewBuffer{};
    if (!NewBuffer.Initialize(mDevice, Description)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
    SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements = NewCapacity;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NewSRV{};
    HRESULT Hr{mDevice->CreateShaderResourceView(NewBuffer.GetBuffer(), &SRVDesc, NewSRV.GetAddressOf())};
    if (FAILED(Hr)) {
        return false;
    }

    mInstanceBuffer = std::move(NewBuffer);
    mInstanceBufferSrv = std::move(NewSRV);
    mInstanceCapacity = NewCapacity;

    return true;
}
