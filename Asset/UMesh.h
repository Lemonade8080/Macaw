#pragma once

#include "Core/Base/UObject.h"
#include "Asset/Pipeline/Defines.h"
#include "Core/Base/FVertexAttribute.h"

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "UAsset.h"
#include "Core/Base/FAssetHandle.h"
#include "Core/Base/TypeInfo.h"

class UMesh : public UAsset {
private:
    struct FVertexAttributeStorageBase { virtual ~FVertexAttributeStorageBase() = default; virtual const void* GetData() const = 0; virtual Uint32 GetCount() const = 0; virtual Uint32 GetStride() const = 0; };

    template <typename T> struct TVertexAttributeStorage final : FVertexAttributeStorageBase { explicit TVertexAttributeStorage(std::span<const T> InData); const void* GetData() const override; Uint32 GetCount() const override; Uint32 GetStride() const override; std::vector<T> mData{}; };

public:
    struct FSubMesh { Uint32 mFirstIndex{0}; Uint32 mIndexCount{0}; Uint32 mMaterialGroupIndex{0}; };

    using FMaterialResolver = std::function<FAssetHandle(const std::filesystem::path& MaterialPath)>;
    using FMaterialGroupResolver = std::function<std::optional<Uint32>(FAssetHandle MaterialHandle, const FString& GroupName)>;

public:
    UMesh() = default;
    ~UMesh() = default;

    UMesh(const UMesh&) = delete;
    UMesh& operator=(const UMesh&) = delete;

    UMesh(UMesh&&) noexcept = default;
    UMesh& operator=(UMesh&&) noexcept = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UMesh, UAsset);

    bool Initialize(ID3D11Device* Device, const std::filesystem::path& SourceObjPath, const std::filesystem::path& BinaryPath, const FMaterialResolver& MaterialResolver, const FMaterialGroupResolver& MaterialGroupResolver, bool FlipUV);

    template <CVertexAttributeView... TAttributes> bool Make(ID3D11Device* Device, const std::span<const Uint32>& InIndices, const TAttributes&... InAttributes);

    ID3D11Buffer* GetVertexBuffer(EVertexAttribute Attribute) const;
    ID3D11Buffer* GetIndexBuffer() const;

    bool HasVertexAttribute(EVertexAttribute Attribute) const;

    Uint32 GetVertexStride(EVertexAttribute Attribute) const;
    Uint32 GetVertexAttributeCount(EVertexAttribute Attribute) const;

    const void* GetVertexData(EVertexAttribute Attribute) const;

    template <EVertexAttribute Attribute> std::span<const TVertexAttributeElementType<Attribute>> GetVertexAttributeData() const;

    const TArray<Uint32>& GetIndices() const;

    const TArray<FSubMesh>& GetSubMeshes() const;

    void SetSubMeshes(const std::span<FSubMesh>& InSubMeshes);

protected:
    virtual void Serialize(FArchive& Ar) override;

private:
    template <typename... TAttributes> static consteval bool AreVertexAttributesUnique();

    template <CVertexAttributeView TAttribute> bool CreateVertexBuffer(ID3D11Device* Device, const TAttribute& InAttribute);

    bool CreateIndexBuffer(ID3D11Device* Device, const std::span<const Uint32>& InIndices);

    void Reset();

    static std::size_t GetAttributeIndex(EVertexAttribute Attribute);

    static std::size_t GetAttributeCount();

private:
    TFixedArray<Microsoft::WRL::ComPtr<ID3D11Buffer>, static_cast<std::size_t>(EVertexAttribute::MAX)> mVertexBuffers{};
    TFixedArray<std::unique_ptr<FVertexAttributeStorageBase>, static_cast<std::size_t>(EVertexAttribute::MAX)> mAttributeStorage{};

    Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer{nullptr};

    TArray<Uint32> mIndices{};

    TArray<FSubMesh> mSubMeshes{};
};

template <typename T> UMesh::TVertexAttributeStorage<T>::TVertexAttributeStorage(std::span<const T> InData)
    : mData(InData.begin(), InData.end()) {
}

template <typename T> const void* UMesh::TVertexAttributeStorage<T>::GetData() const {
    return mData.data();
}

template <typename T> Uint32 UMesh::TVertexAttributeStorage<T>::GetCount() const {
    return static_cast<Uint32>(mData.size());
}

template <typename T> Uint32 UMesh::TVertexAttributeStorage<T>::GetStride() const {
    return static_cast<Uint32>(sizeof(T));
}

template <CVertexAttributeView... TAttributes> bool UMesh::Make(ID3D11Device* Device, const std::span<const Uint32>& InIndices, const TAttributes&... InAttributes) {
    static_assert(sizeof...(TAttributes) > 0, "UMesh requires at least one vertex attribute.");
    static_assert(AreVertexAttributesUnique<TAttributes...>(), "Duplicate vertex attributes are not allowed.");

    Reset();

    if (Device == nullptr || InIndices.empty()) {
        return false;
    }

    Uint32 ExpectedVertexCount{0};
    bool BFirstAttribute{true};
    bool BSuccess{true};

    auto ProcessAttribute{[&](const auto& InAttribute) {
        if (!BSuccess) {
            return;
        }

        if (InAttribute.mData.empty() || InAttribute.mData.size() > std::numeric_limits<Uint32>::max()) {
            BSuccess = false;
            return;
        }

        const Uint32 AttributeVertexCount{static_cast<Uint32>(InAttribute.mData.size())};

        if (BFirstAttribute) {
            ExpectedVertexCount = AttributeVertexCount;
            BFirstAttribute = false;
        } else if (AttributeVertexCount != ExpectedVertexCount) {
            BSuccess = false;
            return;
        }

        if (!CreateVertexBuffer(Device, InAttribute)) {
            BSuccess = false;
        }
    }};

    (ProcessAttribute(InAttributes), ...);

    if (!BSuccess) {
        Reset();
        return false;
    }

    if (!CreateIndexBuffer(Device, InIndices)) {
        Reset();
        return false;
    }

    mIndices.assign(InIndices.begin(), InIndices.end());

    return true;
}

template <EVertexAttribute Attribute> std::span<const TVertexAttributeElementType<Attribute>> UMesh::GetVertexAttributeData() const {
    using ElementType = TVertexAttributeElementType<Attribute>;
    using StorageType = TVertexAttributeStorage<ElementType>;

    constexpr std::size_t Index{static_cast<std::size_t>(Attribute)};

    if (!mAttributeStorage[Index]) {
        return {};
    }

    const StorageType* Storage{static_cast<const StorageType*>(mAttributeStorage[Index].get())};

    return std::span<const ElementType>{Storage->mData.data(), Storage->mData.size()};
}

template <typename... TAttributes> consteval bool UMesh::AreVertexAttributesUnique() {
    constexpr std::array<EVertexAttribute, sizeof...(TAttributes)> Attributes{std::remove_cvref_t<TAttributes>::AttributeType...};

    for (std::size_t I{0}; I < Attributes.size(); ++I) {
        for (std::size_t J{I + 1}; J < Attributes.size(); ++J) {
            if (Attributes[I] == Attributes[J]) {
                return false;
            }
        }
    }

    return true;
}

template <CVertexAttributeView TAttribute> bool UMesh::CreateVertexBuffer(ID3D11Device* Device, const TAttribute& InAttribute) {
    using AttributeType = std::remove_cvref_t<TAttribute>;
    using ElementType = typename AttributeType::ElementType;

    constexpr EVertexAttribute Attribute{AttributeType::AttributeType};
    constexpr std::size_t AttributeIndex{static_cast<std::size_t>(Attribute)};

    if (InAttribute.mData.empty() || InAttribute.mData.size_bytes() > std::numeric_limits<UINT>::max()) {
        return false;
    }

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth = static_cast<UINT>(InAttribute.mData.size_bytes());
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;
    BufferDesc.MiscFlags = 0;
    BufferDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = InAttribute.mData.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer{};

    const HRESULT Result{Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf())};

    if (FAILED(Result)) {
        return false;
    }

    mVertexBuffers[AttributeIndex] = std::move(Buffer);
    mAttributeStorage[AttributeIndex] = std::make_unique<TVertexAttributeStorage<ElementType>>(InAttribute.mData);

    return true;
}
