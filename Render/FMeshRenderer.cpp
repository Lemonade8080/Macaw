#include "pch.h"
#include "FMeshRenderer.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMesh.h"
#include "Asset/UTexture.h"
#include "FFrameResource.h"
#include "RenderConfig.h"
#include "Core/Stat/Stat.h"

void FMeshRenderer::Draw(const FRenderContext& Context, const TArray<FMeshDrawBatch>& Items, ERenderMode Mode) {
    mLastDrawStats = {};
    if (Items.empty() || Context.mDeviceContext == nullptr || Context.mAssetRegistry == nullptr || Context.mFrameResource == nullptr) {
        return;
    }

    ID3D11DeviceContext* DeviceContext{Context.mDeviceContext};
    if (!Context.mFrameResource->BindModels(DeviceContext)) {
        return;
    }

    DeviceContext->VSSetShaderResources(1, 1, &Context.mMaterialResource);
    DeviceContext->PSSetShaderResources(1, 1, &Context.mMaterialResource);

    const UPipeline* BoundPipeline{};
    ERenderMode BoundMode{ERenderMode::Lit};
    Uint32 BoundStencilReference{};
    const FMaterialChunkSignature* BoundTextures{};
    const UMesh* BoundMesh{};
    Uint32 BoundLOD{};

    for (const FMeshDrawBatch& Item : Items) {
        const FMeshDrawState& State{Item.mState};
        const UPipeline* Pipeline{Context.mAssetRegistry->ResolveAsset<UPipeline>(State.mPipelineHandle)};
        const UMesh* Mesh{Context.mAssetRegistry->ResolveAsset<UMesh>(State.mMeshHandle)};
        if (Pipeline == nullptr || Mesh == nullptr || Item.mRecordCount == 0 || (Mode == ERenderMode::Outline && !Pipeline->RenderModeSettable(Mode))) {
            continue;
        }

        const ERenderMode ResolvedMode{Pipeline->ResolveRenderMode(Mode)};
        if (!Pipeline->RenderModeSettable(ResolvedMode)) {
            continue;
        }

        const Uint32 StencilReference{ResolvedMode == ERenderMode::Outline || (Item.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0 ? 1u : 0u};
        if (BoundPipeline != Pipeline || BoundMode != ResolvedMode || BoundStencilReference != StencilReference) {
            Pipeline->Bind(DeviceContext, ResolvedMode, StencilReference);
            BoundPipeline = Pipeline;
            BoundMode = ResolvedMode;
            BoundStencilReference = StencilReference;
            ++mLastDrawStats.mPipelineBindCount;
        }

        if (BoundTextures == nullptr || *BoundTextures != State.mTextureSignature) {
            std::array<ID3D11ShaderResourceView*, MaxMaterialTextureFields> TextureResources{};
            for (Uint8 Index{}; Index < State.mTextureSignature.mTextureFieldCount; ++Index) {
                const UTexture* Texture{Context.mAssetRegistry->ResolveAsset<UTexture>(State.mTextureSignature.GetTextureHandle(Index))};
                TextureResources[Index] = Texture != nullptr ? Texture->GetSRV() : nullptr;
            }

            DeviceContext->VSSetShaderResources(3, static_cast<UINT>(TextureResources.size()), TextureResources.data());
            DeviceContext->PSSetShaderResources(3, static_cast<UINT>(TextureResources.size()), TextureResources.data());
            BoundTextures = &State.mTextureSignature;
            ++mLastDrawStats.mTextureBindCount;
        }

        const int LODLevel{static_cast<int>(State.mLODLevel)};
        if (BoundMesh != Mesh || BoundLOD != State.mLODLevel) {
            ID3D11Buffer* VertexBuffers[]{Mesh->GetVertexBuffer(EVertexAttribute::Position, LODLevel), Mesh->GetVertexBuffer(EVertexAttribute::Normal, LODLevel), Mesh->GetVertexBuffer(EVertexAttribute::UV, LODLevel), Mesh->GetVertexBuffer(EVertexAttribute::Color, LODLevel)};
            const Uint32 Strides[]{Mesh->GetVertexStride(EVertexAttribute::Position), Mesh->GetVertexStride(EVertexAttribute::Normal), Mesh->GetVertexStride(EVertexAttribute::UV), Mesh->GetVertexStride(EVertexAttribute::Color)};
            const Uint32 Offsets[]{0, 0, 0, 0};
            DeviceContext->IASetVertexBuffers(0, _countof(VertexBuffers), VertexBuffers, Strides, Offsets);
            DeviceContext->IASetIndexBuffer(Mesh->GetIndexBuffer(LODLevel), DXGI_FORMAT_R32_UINT, 0);
            BoundMesh = Mesh;
            BoundLOD = State.mLODLevel;
            ++mLastDrawStats.mMeshBindCount;
        }

        const Uint32 OriginalIndexCount{LODLevel > 0 ? Mesh->GetIndexCount(0) : State.mIndexCount};
#if ENABLE_INSTANCE
        DeviceContext->DrawIndexedInstanced(State.mIndexCount, Item.mRecordCount, State.mFirstIndex, 0, Item.mFirstRecord);
        ++mLastDrawStats.mDrawCallCount;
        Stat::RecordLODStats(State.mLODLevel, static_cast<std::uint64_t>(State.mIndexCount / 3) * Item.mRecordCount, static_cast<std::uint64_t>(OriginalIndexCount / 3) * Item.mRecordCount, 1);
#else
        Uint32 DrawCount{};
        for (Uint32 Index{}; Index < Item.mRecordCount; ++Index) {
            if (Context.mFrameResource->BindMeshDraw(DeviceContext, Item.mFirstRecord + Index)) {
                DeviceContext->DrawIndexed(State.mIndexCount, State.mFirstIndex, 0);
                ++DrawCount;
            }
        }

        mLastDrawStats.mDrawCallCount += DrawCount;
        Stat::RecordLODStats(State.mLODLevel, static_cast<std::uint64_t>(State.mIndexCount / 3) * DrawCount, static_cast<std::uint64_t>(OriginalIndexCount / 3) * DrawCount, DrawCount);
#endif
    }
}

const FMeshDrawStats& FMeshRenderer::GetLastDrawStats() const {
    return mLastDrawStats;
}
