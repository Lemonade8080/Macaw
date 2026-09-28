#pragma once

struct FGuid {
    Uint32 mA{0};
    Uint32 mB{0};
    Uint32 mC{0};
    Uint32 mD{0};

    static FGuid NewGuid();
    FString ToString() const;
    bool Parse(const FString& GuidString);

    bool IsValid() const;

    std::size_t GetHash() const noexcept;

    bool operator==(const FGuid& Other) const;

    bool operator!=(const FGuid& Other) const;
};

namespace std {
    template <> struct hash<FGuid> { std::size_t operator()(const FGuid& Guid) const noexcept; };
}
