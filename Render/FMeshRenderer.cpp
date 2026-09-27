#include "pch.h"
#include "FMeshRenderer.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMesh.h"
#include "Asset/UTexture.h"

bool FMeshRenderer::Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context) {
    return mModelContextArray.Initialize(Device, Context, 128) && mRootConstants.Initialize(Device);
}

void FMeshRenderer::Draw(const FRenderContext& Context, const TArray<FMeshDrawItem>& Items, const CameraProbe& Camera, ERenderMode Mode) {
    if (Items.empty() || Context.mAssetRegistry == nullptr) {
        return;
    }
    mModelContexts.clear();
    mModelContexts.reserve(Items.size());
    for (const FMeshDrawItem& Item : Items) {
        mModelContexts.push_back(FModelContext{Item.mProbe.mWorld, Item.mMaterialIndex, Item.mProbe.mFlags});
    }
    ID3D11DeviceContext* DeviceContext{Context.mDeviceContext};
    ID3D11ShaderResourceView* NullResource{nullptr};
    DeviceContext->VSSetShaderResources(0, 1, &NullResource);
    DeviceContext->PSSetShaderResources(0, 1, &NullResource);
    if (!mModelContextArray.UploadDiscard(Context.mDevice, DeviceContext, mModelContexts)) {
        return;
    }
    DeviceContext->VSSetShaderResources(0, 1, mModelContextArray.GetSRV());
    DeviceContext->PSSetShaderResources(0, 1, mModelContextArray.GetSRV());
    DeviceContext->VSSetShaderResources(1, 1, &Context.mMaterialResource);
    DeviceContext->PSSetShaderResources(1, 1, &Context.mMaterialResource);
    DeviceContext->PSSetShaderResources(2, 1, &Context.mLightResource);

    struct FCameraData {
        FMatrix mView{};
        FMatrix mProjection{};
        FMatrix mViewProjection{};
    };
    mRootConstants.SetGraphicsRoot32BitConstants(FCameraData{Camera.mView, Camera.mProjection, Camera.mViewProjection}, 0);
    mRootConstants.SetGraphicsRoot32BitConstant(Context.mLightCount, 49);
    mRootConstants.SetGraphicsRoot32BitConstant(Context.mAnimationFrame, 50);
    UINT ViewportCount{1};
    D3D11_VIEWPORT Viewport{};
    DeviceContext->RSGetViewports(&ViewportCount, &Viewport);
    const FVector4 ViewportConstants{Viewport.Width, Viewport.Height, Viewport.Width > 0.0f ? 1.0f / Viewport.Width : 0.0f, Viewport.Height > 0.0f ? 1.0f / Viewport.Height : 0.0f};
    mRootConstants.SetGraphicsRoot32BitConstants(ViewportConstants, 52);
    mRootConstants.Bind(DeviceContext, 0, EGraphicsShaderStage::Graphics);

    for (std::size_t Begin{}; Begin < Items.size();) {
        const FMeshDrawItem& First{Items[Begin]};
        std::size_t End{Begin + 1};
        while (End < Items.size() && First.HasSameBatch(Items[End]) && ((First.mProbe.mFlags ^ Items[End].mProbe.mFlags) & static_cast<Uint32>(ERenderObjectFlags::Selected)) == 0) {
            ++End;
        }
        const UPipeline* Pipeline{Context.mAssetRegistry->ResolveAsset<UPipeline>(First.mProbe.mPipelineHandle)};
        const UMesh* Mesh{Context.mAssetRegistry->ResolveAsset<UMesh>(First.mProbe.mMeshHandle)};
        if (Pipeline != nullptr && Mesh != nullptr && (Mode != ERenderMode::Outline || Pipeline->RenderModeSettable(Mode))) {
            const ERenderMode ResolvedMode{Pipeline->ResolveRenderMode(Mode)};
            const bool LitWireframe{ResolvedMode == ERenderMode::LitWireframe && Pipeline->RenderModeSettable(ERenderMode::Lit)};
            const UINT StencilReference{ResolvedMode == ERenderMode::Outline || (First.mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0 ? 1u : 0u};
            Pipeline->Bind(DeviceContext, LitWireframe ? ERenderMode::Lit : ResolvedMode, StencilReference);
            std::array<ID3D11ShaderResourceView*, MaxMaterialTextureFields> TextureResources{};
            for (Uint8 Index{}; Index < First.mTextureSignature.mTextureFieldCount; ++Index) {
                const UTexture* Texture{Context.mAssetRegistry->ResolveAsset<UTexture>(First.mTextureSignature.GetTextureHandle(Index))};
                TextureResources[Index] = Texture != nullptr ? Texture->GetSRV() : nullptr;
            }
            DeviceContext->VSSetShaderResources(3, static_cast<UINT>(TextureResources.size()), TextureResources.data());
            DeviceContext->PSSetShaderResources(3, static_cast<UINT>(TextureResources.size()), TextureResources.data());

            ID3D11Buffer* VertexBuffers[]{Mesh->GetVertexBuffer(EVertexAttribute::Position), Mesh->GetVertexBuffer(EVertexAttribute::Normal), Mesh->GetVertexBuffer(EVertexAttribute::UV), Mesh->GetVertexBuffer(EVertexAttribute::Color)};
            const Uint32 Strides[]{Mesh->GetVertexStride(EVertexAttribute::Position), Mesh->GetVertexStride(EVertexAttribute::Normal), Mesh->GetVertexStride(EVertexAttribute::UV), Mesh->GetVertexStride(EVertexAttribute::Color)};
            const Uint32 Offsets[]{0, 0, 0, 0};
            DeviceContext->IASetVertexBuffers(0, _countof(VertexBuffers), VertexBuffers, Strides, Offsets);
            DeviceContext->IASetIndexBuffer(Mesh->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
            mRootConstants.SetGraphicsRoot32BitConstant(static_cast<Uint32>(Begin), 48);
            mRootConstants.Commit(DeviceContext);
            DeviceContext->DrawIndexedInstanced(First.mIndexCount, static_cast<Uint32>(End - Begin), First.mFirstIndex, 0, 0);
            if (LitWireframe) {
                Pipeline->Bind(DeviceContext, ERenderMode::LitWireframe, StencilReference);
                DeviceContext->DrawIndexedInstanced(First.mIndexCount, static_cast<Uint32>(End - Begin), First.mFirstIndex, 0, 0);
            }
        }
        Begin = End;
    }
}

void FMeshRenderer::Reset() {
    mModelContextArray.Reset();
    mRootConstants.Reset();
    mModelContexts.clear();
}
