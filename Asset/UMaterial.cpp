#include "pch.h"
#include "UMaterial.h"

void UMaterial::BuildGPUData(Uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const {
    if (GroupIndex != 0) {
        OutSlot = {};
        return;
    }

    BuildGPUData(OutSlot);
}

FMaterialChunkSignature UMaterial::BuildChunkSignature() const {
    return {};
}

FMaterialChunkSignature UMaterial::BuildChunkSignature(Uint32 GroupIndex) const {
    return GroupIndex == 0 ? BuildChunkSignature() : FMaterialChunkSignature{};
}

void UMaterial::Finalize(const IAssetRegistry* Query) {
}

std::optional<Uint32> UMaterial::FindGroupIndex(const FString& Name) const {
    return std::nullopt;
}

void UMaterial::Serialize(FArchive& Ar) {
    UAsset::Serialize(Ar);
}

Uint32 UMaterial::GetGPUIndex() const {
    return mGpuIndices.empty() ? UINT32_MAX : mGpuIndices[0];
}

Uint32 UMaterial::GetGPUIndex(Uint32 GroupIndex) const {
    return GroupIndex < mGpuIndices.size() ? mGpuIndices[GroupIndex] : UINT32_MAX;
}

std::span<const Uint32> UMaterial::GetMaterialIndices() const {
    return mGpuIndices;
}

Uint32 UMaterial::GetGPUDataCount() const {
    return 1;
}

void UMaterial::MarkGPUDataDirty() {
    ++mRenderRevision;
    mBGpuDataDirty = true;
}

Uint64 UMaterial::GetRenderRevision() const {
    return mRenderRevision;
}
