#pragma once

#include "Core/STL.h"

struct FAssetPath {
    FString mPath{};

    FAssetPath() = default;

    explicit FAssetPath(const FString& InPath);

    explicit FAssetPath(FString&& InPath);

    bool IsValid() const;

    explicit operator bool() const;

    bool operator==(const FAssetPath& Other) const = default;
};

namespace std {
    template <> struct hash<FAssetPath> { std::size_t operator()(const FAssetPath& AssetPath) const noexcept; };
}
