#pragma once

#include "FMaterialChunkSignature.h"
#include "FMaterialGPUData.h"
#include "UAsset.h"
#include "Core/Asset/IAssetRegistry.h"

#include <optional>
#include <span>

class UMaterial : public UAsset {
public:
    UMaterial() = default;
    virtual ~UMaterial() = default;

    UMaterial(const UMaterial&) = delete;
    UMaterial& operator=(const UMaterial&) = delete;

    UMaterial(UMaterial&&) noexcept = default;
    UMaterial& operator=(UMaterial&&) noexcept = default;

public:
    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UMaterial, UAsset);

    virtual void BuildGPUData(FMaterialGPUSlot& OutSlot) const = 0;
    virtual void BuildGPUData(Uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const;
    virtual FMaterialChunkSignature BuildChunkSignature() const;
    virtual FMaterialChunkSignature BuildChunkSignature(Uint32 GroupIndex) const;
    virtual void Finalize(const IAssetRegistry* Query);

    Uint32 GetGPUIndex() const;

    Uint32 GetGPUIndex(Uint32 GroupIndex) const;

    std::span<const Uint32> GetMaterialIndices() const;

    virtual Uint32 GetGPUDataCount() const;

    virtual std::optional<Uint32> FindGroupIndex(const FString& Name) const;

    void MarkGPUDataDirty();
    Uint64 GetRenderRevision() const;

protected:
    void Serialize(FArchive& Ar) override;

private:
    friend class FMaterialBuffer;

    TArray<Uint32> mGpuIndices{};
    Uint64 mRenderRevision{1};
    bool mBGpuDataDirty{true};
};
