#pragma once

#include "FGraphicsBuffer.h"

#include <algorithm>
#include <span>
#include <type_traits>

template <typename T, bool BAutoResize = true, bool BDynamic = false>
class TGraphicsArray {
    static_assert(std::is_trivially_copyable_v<T>, "TGraphicsArray requires trivially copyable element types.");
    static_assert(sizeof(T) % 4 == 0, "Structured buffer element size must be aligned to 4 bytes.");

public:
    TGraphicsArray() = default;
    ~TGraphicsArray() = default;

    TGraphicsArray(const TGraphicsArray&) = delete;
    TGraphicsArray& operator=(const TGraphicsArray&) = delete;

    TGraphicsArray(TGraphicsArray&&) noexcept = default;
    TGraphicsArray& operator=(TGraphicsArray&&) noexcept = default;

public:
    bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 InCapacity, std::span<const T> InitialData = {});

    bool UploadDiscard(ID3D11Device* Device, ID3D11DeviceContext* Context, std::span<const T> Values) requires(BDynamic);
    bool UploadNoOverwrite(ID3D11Device* Device, ID3D11DeviceContext* Context, std::span<const T> Values) requires(BDynamic);

    bool Add(ID3D11Device* Device, ID3D11DeviceContext* Context, const T& Value);

    bool AddRange(ID3D11Device* Device, ID3D11DeviceContext* Context, std::span<const T> Values);

    bool Update(ID3D11DeviceContext* Context, Uint32 Index, const T& Value);

    bool UpdateRange(ID3D11DeviceContext* Context, Uint32 StartIndex, std::span<const T> Values);

    bool Reserve(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 NewCapacity);

    bool PopBack();

    void Clear();

    void Reset();

public:
    [[nodiscard]] ID3D11Buffer* GetBuffer() const;

    [[nodiscard]] ID3D11ShaderResourceView* const* GetSRV() const;

    [[nodiscard]] Uint32 GetCount() const;

    [[nodiscard]] Uint32 GetCapacity() const;

    [[nodiscard]] Uint32 GetStride() const;

    [[nodiscard]] Uint32 GetByteSize() const;

    [[nodiscard]] bool IsEmpty() const;

    [[nodiscard]] bool IsFull() const;

    [[nodiscard]] bool IsValid() const;

private:
    bool EnsureCapacity(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 RequiredCapacity);

    bool Resize(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 NewCapacity);

    bool CreateSRV(ID3D11Device* Device, ID3D11Buffer* InBuffer, Uint32 InCapacity, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& OutSRV);

private:
    FGraphicsBuffer mBuffer{};
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSrv{};

    Uint32 mCount{0};
    Uint32 mCapacity{0};
};

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 InCapacity, std::span<const T> InitialData) {
    if (!Device || !Context || InCapacity == 0 || InitialData.size() > InCapacity) {
        return false;
    }

    Reset();

    FGraphicsBufferDescription Description{};
    Description.mByteSize = InCapacity * sizeof(T);
    Description.mStride = sizeof(T);
    Description.mUsage = BDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    Description.mBindFlags = D3D11_BIND_SHADER_RESOURCE;
    Description.mCpuAccessFlags = BDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    Description.mMiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    if (!mBuffer.Initialize(Device, Description)) {
        return false;
    }

    if (!CreateSRV(Device, mBuffer.GetBuffer(), InCapacity, mSrv)) {
        Reset();
        return false;
    }

    mCapacity = InCapacity;

    if (!InitialData.empty()) {
        const bool BUploaded{BDynamic ? mBuffer.WriteDiscard(Context, InitialData.data(), static_cast<Uint32>(InitialData.size_bytes())) : mBuffer.Update(Context, InitialData.data(), static_cast<Uint32>(InitialData.size_bytes()))};
        if (!BUploaded) {
            Reset();
            return false;
        }

        mCount = static_cast<Uint32>(InitialData.size());
    }

    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::UploadDiscard(ID3D11Device* Device, ID3D11DeviceContext* Context, std::span<const T> Values) requires(BDynamic) {
    if (Values.empty()) {
        mCount = 0;
        return true;
    }

    if (!EnsureCapacity(Device, Context, static_cast<Uint32>(Values.size()))) {
        return false;
    }

    if (!mBuffer.WriteDiscard(Context, Values.data(), static_cast<Uint32>(Values.size_bytes()))) {
        return false;
    }

    mCount = static_cast<Uint32>(Values.size());
    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::UploadNoOverwrite(ID3D11Device* Device, ID3D11DeviceContext* Context, std::span<const T> Values) requires(BDynamic) {
    if (Device == nullptr || Context == nullptr || Values.size() > UINT32_MAX / sizeof(T)) {
        return false;
    }
    if (Values.empty()) {
        mCount = 0;
        return true;
    }
    if (!EnsureCapacity(Device, Context, static_cast<Uint32>(Values.size())) || !mBuffer.WriteNoOverwrite(Context, Values.data(), static_cast<Uint32>(Values.size_bytes()), 0)) {
        return false;
    }
    mCount = static_cast<Uint32>(Values.size());
    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::Add(ID3D11Device* Device, ID3D11DeviceContext* Context, const T& Value) {
    if (!EnsureCapacity(Device, Context, mCount + 1)) {
        return false;
    }

    if (!mBuffer.Update(Context, &Value, sizeof(T), mCount * sizeof(T))) {
        return false;
    }

    ++mCount;

    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::AddRange(ID3D11Device* Device, ID3D11DeviceContext* Context, std::span<const T> Values) {
    if (Values.empty()) {
        return true;
    }

    const Uint32 AddCount{static_cast<Uint32>(Values.size())};

    if (AddCount > UINT32_MAX - mCount) {
        return false;
    }

    const Uint32 RequiredCapacity{mCount + AddCount};

    if (!EnsureCapacity(Device, Context, RequiredCapacity)) {
        return false;
    }

    if (!mBuffer.Update(Context, Values.data(), static_cast<Uint32>(Values.size_bytes()), mCount * sizeof(T))) {
        return false;
    }

    mCount = RequiredCapacity;

    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::Update(ID3D11DeviceContext* Context, Uint32 Index, const T& Value) {
    if (Index >= mCount) {
        return false;
    }

    return mBuffer.Update(Context, &Value, sizeof(T), Index * sizeof(T));
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::UpdateRange(ID3D11DeviceContext* Context, Uint32 StartIndex, std::span<const T> Values) {
    if (Values.empty()) {
        return true;
    }

    if (StartIndex > mCount || Values.size() > mCount - StartIndex) {
        return false;
    }

    return mBuffer.Update(Context, Values.data(), static_cast<Uint32>(Values.size_bytes()), StartIndex * sizeof(T));
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::Reserve(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 NewCapacity) {
    if (NewCapacity <= mCapacity) {
        return true;
    }

    return Resize(Device, Context, NewCapacity);
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::PopBack() {
    if (mCount == 0) {
        return false;
    }

    --mCount;

    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> void TGraphicsArray<T, BAutoResize, BDynamic>::Clear() {
    mCount = 0;
}

template <typename T, bool BAutoResize, bool BDynamic> void TGraphicsArray<T, BAutoResize, BDynamic>::Reset() {
    mBuffer.Reset();
    mSrv.Reset();

    mCount = 0;
    mCapacity = 0;
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] ID3D11Buffer* TGraphicsArray<T, BAutoResize, BDynamic>::GetBuffer() const {
    return mBuffer.GetBuffer();
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] ID3D11ShaderResourceView* const* TGraphicsArray<T, BAutoResize, BDynamic>::GetSRV() const {
    return mSrv.GetAddressOf();
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] Uint32 TGraphicsArray<T, BAutoResize, BDynamic>::GetCount() const {
    return mCount;
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] Uint32 TGraphicsArray<T, BAutoResize, BDynamic>::GetCapacity() const {
    return mCapacity;
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] Uint32 TGraphicsArray<T, BAutoResize, BDynamic>::GetStride() const {
    return sizeof(T);
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] Uint32 TGraphicsArray<T, BAutoResize, BDynamic>::GetByteSize() const {
    return mCapacity * sizeof(T);
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] bool TGraphicsArray<T, BAutoResize, BDynamic>::IsEmpty() const {
    return mCount == 0;
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] bool TGraphicsArray<T, BAutoResize, BDynamic>::IsFull() const {
    return mCount >= mCapacity;
}

template <typename T, bool BAutoResize, bool BDynamic> [[nodiscard]] bool TGraphicsArray<T, BAutoResize, BDynamic>::IsValid() const {
    return mBuffer.IsValid() && mSrv != nullptr;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::EnsureCapacity(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 RequiredCapacity) {
    if (RequiredCapacity <= mCapacity) {
        return true;
    }

    if constexpr (!BAutoResize) {
        return false;
    }

    Uint32 NewCapacity{mCapacity == 0 ? 1 : mCapacity};

    while (NewCapacity < RequiredCapacity) {
        if (NewCapacity > UINT32_MAX / 2) {
            NewCapacity = RequiredCapacity;
            break;
        }

        NewCapacity *= 2;
    }

    return Resize(Device, Context, NewCapacity);
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::Resize(ID3D11Device* Device, ID3D11DeviceContext* Context, Uint32 NewCapacity) {
    if (!Device || !Context || NewCapacity <= mCapacity || NewCapacity < mCount) {
        return false;
    }

    if (NewCapacity > UINT32_MAX / sizeof(T)) {
        return false;
    }

    FGraphicsBufferDescription Description{};
    Description.mByteSize = NewCapacity * sizeof(T);
    Description.mStride = sizeof(T);
    Description.mUsage = BDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    Description.mBindFlags = D3D11_BIND_SHADER_RESOURCE;
    Description.mCpuAccessFlags = BDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    Description.mMiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    FGraphicsBuffer NewBuffer{};

    if (!NewBuffer.Initialize(Device, Description)) {
        return false;
    }

    if constexpr (!BDynamic) {
        if (mBuffer.IsValid() && mCount > 0) {
            if (!NewBuffer.CopyFrom(Context, 0, mBuffer, 0, mCount * sizeof(T))) {
                return false;
            }
        }
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NewSRV{};

    if (!CreateSRV(Device, NewBuffer.GetBuffer(), NewCapacity, NewSRV)) {
        return false;
    }

    mBuffer = std::move(NewBuffer);
    mSrv = std::move(NewSRV);
    mCapacity = NewCapacity;

    return true;
}

template <typename T, bool BAutoResize, bool BDynamic> bool TGraphicsArray<T, BAutoResize, BDynamic>::CreateSRV(ID3D11Device* Device, ID3D11Buffer* InBuffer, Uint32 InCapacity, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& OutSRV) {
    if (!Device || !InBuffer || InCapacity == 0) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC Description{};
    Description.Format = DXGI_FORMAT_UNKNOWN;
    Description.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    Description.Buffer.FirstElement = 0;
    Description.Buffer.NumElements = InCapacity;

    return SUCCEEDED(Device->CreateShaderResourceView(InBuffer, &Description, OutSRV.ReleaseAndGetAddressOf()));
}
