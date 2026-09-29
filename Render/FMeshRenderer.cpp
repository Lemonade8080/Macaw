#include "pch.h"
#include "FMeshRenderer.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMesh.h"
#include "Asset/UTexture.h"
#include "FFrameResource.h"


// #define ENABLE_INSTANCE 

void FMeshRenderer::Draw(const FRenderContext& Context, const TArray<FMeshDrawItem>& Items, ERenderMode Mode) {
    if (Items.empty() || Context.mAssetRegistry == nullptr || Context.mFrameResource == nullptr) {
        return;
    }
    ID3D11DeviceContext* DeviceContext{Context.mDeviceContext};
    if (!Context.mFrameResource->BindModels(DeviceContext)) {
        return;
    }
    DeviceContext->VSSetShaderResources(1, 1, &Context.mMaterialResource);
    DeviceContext->PSSetShaderResources(1, 1, &Context.mMaterialResource);

    for (std::size_t Begin{}; Begin < Items.size();) {
        const FMeshDrawItem& First{Items[Begin]};
        std::size_t End{Begin + 1};
        while (End < Items.size() && First.HasSameBatch(Items[End]) && Items[End].mModelIndex == static_cast<std::size_t>(First.mModelIndex) + End - Begin && ((First.mProbe.mFlags ^ Items[End].mProbe.mFlags) & static_cast<Uint32>(ERenderObjectFlags::Selected)) == 0) {
            ++End;
        }
        const UPipeline* Pipeline{Context.mAssetRegistry->ResolveAsset<UPipeline>(First.mProbe.mPipelineHandle)};
        const UMesh* Mesh{Context.mAssetRegistry->ResolveAsset<UMesh>(First.mProbe.mMeshHandle)};
        if (Pipeline != nullptr && Mesh != nullptr && (Mode != ERenderMode::Outline || Pipeline->RenderModeSettable(Mode))) {
            const ERenderMode ResolvedMode{Pipeline->ResolveRenderMode(Mode)};
            const UINT StencilReference{ResolvedMode == ERenderMode::Outline || (First.mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0 ? 1u : 0u};
            Pipeline->Bind(DeviceContext, ResolvedMode, StencilReference);
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
#ifdef ENABLE_INSTANCE
            DeviceContext->DrawIndexedInstanced(First.mIndexCount, static_cast<Uint32>(End - Begin), First.mFirstIndex, 0, First.mModelIndex);
#else
            for (std::size_t Index{Begin}; Index < End; ++Index) {
                if (Context.mFrameResource->BindMeshDraw(DeviceContext, Items[Index].mModelIndex)) {
                    DeviceContext->DrawIndexed(First.mIndexCount, First.mFirstIndex, 0);
                }
            }
#endif
        }
        Begin = End;
    }
}
