#include "pch.h"
#include "FBillboardRenderer.h"

#include "Asset/Pipeline/UPipeline.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UTexture.h"
#include "FFrameResource.h"
#include <algorithm>
#include <unordered_map>

bool FBillboardRenderer::FBatchKey::operator==(const FBatchKey& Other) const {
    return mPipelineHandle == Other.mPipelineHandle && mTextureHandle == Other.mTextureHandle;
}

std::size_t FBillboardRenderer::FBatchKeyHash::operator()(const FBatchKey& Key) const noexcept {
    return std::hash<std::uint32_t>{}(Key.mPipelineHandle.mId) ^ (std::hash<std::uint32_t>{}(Key.mTextureHandle.mId) << 1);
}

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

    mInstances.clear();
    mDraws.clear();
    if (BillboardProbe.size() > UINT32_MAX / sizeof(FBillboardData)) {
        return;
    }
    std::unordered_map<FBatchKey, FBillboardBatch, FBatchKeyHash> Batches{};
    for (const FBillboardProbe& Probe : BillboardProbe) {
        if (!Probe.mPipelineHandle || !Probe.mTextureHandle) {
            continue;
        }
        ++Batches[FBatchKey{Probe.mPipelineHandle, Probe.mTextureHandle}].mDraw.mInstanceCount;
    }

    Uint32 InstanceCount{};
    for (auto& [Key, Batch] : Batches) {
        const UPipeline* Pipeline{AssetRegistry->ResolveAsset<UPipeline>(Key.mPipelineHandle)};
        const UTexture* Texture{AssetRegistry->ResolveAsset<UTexture>(Key.mTextureHandle)};
        if (Pipeline == nullptr || Texture == nullptr) {
            continue;
        }
        Batch.mDraw.mPipeline = Pipeline;
        Batch.mDraw.mTexture = Texture;
        Batch.mDraw.mFirstInstance = InstanceCount;
        InstanceCount += Batch.mDraw.mInstanceCount;
        mDraws.push_back(Batch.mDraw);
    }
    mInstances.resize(InstanceCount);
    for (const FBillboardProbe& Probe : BillboardProbe) {
        const auto Iterator{Batches.find(FBatchKey{Probe.mPipelineHandle, Probe.mTextureHandle})};
        if (Iterator == Batches.end() || Iterator->second.mDraw.mPipeline == nullptr) {
            continue;
        }
        FBillboardBatch& Batch{Iterator->second};
        mInstances[Batch.mDraw.mFirstInstance + Batch.mWriteCount] = FBillboardData{Probe.mWorld, Probe.mSize, Probe.mUvMin, Probe.mUvMax, FVector2{}, Probe.mColor};
        ++Batch.mWriteCount;
    }
    if (mDraws.empty() || !FrameResource.UploadStream(mDevice, Context, EFrameStream::Billboard, mInstances.data(), static_cast<Uint32>(mInstances.size()), sizeof(FBillboardData), D3D11_BIND_VERTEX_BUFFER)) {
        return;
    }
    ID3D11Buffer* Buffer{FrameResource.GetStreamBuffer(EFrameStream::Billboard)};
    const UINT Stride{sizeof(FBillboardData)};
    const UINT Offset{};
    Context->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    for (const FBillboardDraw& Draw : mDraws) {
        Draw.mPipeline->Bind(Context, Draw.mPipeline->ResolveRenderMode(Mode));
        ID3D11ShaderResourceView* TextureSRV{Draw.mTexture->GetSRV()};
        Context->PSSetShaderResources(3, 1, &TextureSRV);
        Context->DrawInstanced(1, Draw.mInstanceCount, 0, Draw.mFirstInstance);
    }
}
