#pragma once

#include "Core/Base/FAssetHandle.h"

#include <array>
#include <functional>

inline constexpr Uint8 MaxMaterialTextureFields{12};

struct FMaterialChunkSignature {
    std::array<FAssetHandle, MaxMaterialTextureFields> mTextureHandles{};
    Uint8 mTextureFieldCount{0};

    bool IsValid() const;
    FAssetHandle GetTextureHandle(Uint8 TextureFieldIndex) const;
    std::size_t GetHash() const noexcept;

    bool operator==(const FMaterialChunkSignature& Other) const = default;
    bool operator!=(const FMaterialChunkSignature& Other) const = default;
};

class FMaterialChunkSignatureBuilder {
public:
    bool AddTexture(FAssetHandle TextureHandle);

    FMaterialChunkSignature Build() const;
    void Reset();

private:
    FMaterialChunkSignature mSignature{};
};

namespace std {
    template <> struct hash<FMaterialChunkSignature> { std::size_t operator()(const FMaterialChunkSignature& Signature) const noexcept; };
}
