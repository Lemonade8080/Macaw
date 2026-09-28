#pragma once

#include <cstddef>
#include <type_traits>

#include "FObjectHandle.h"
#include "UObject.h"
#include "UObjectSystem.h"

template <typename T>
class TObjectRef {
public:
    TObjectRef() = default;

    TObjectRef(std::nullptr_t) noexcept;

    TObjectRef(T* Object) noexcept;

    explicit TObjectRef(FObjectHandle InHandle) noexcept;

    T* Get() const noexcept;

    T* operator->() const noexcept;

    T& operator*() const noexcept;

    explicit operator bool() const noexcept;

    bool IsValid() const noexcept;

    void Set(T* Object) noexcept;

    void SetHandle(FObjectHandle InHandle) noexcept;

    void Reset() noexcept;

    FObjectHandle GetHandle() const noexcept;

private:
    static constexpr void ValidateType() noexcept;

    FObjectHandle mHandle{};
};

template <typename T> TObjectRef<T>::TObjectRef(std::nullptr_t) noexcept {
}

template <typename T> TObjectRef<T>::TObjectRef(T* Object) noexcept {
    Set(Object);
}

template <typename T> TObjectRef<T>::TObjectRef(FObjectHandle InHandle) noexcept
    : mHandle(InHandle) {
}

template <typename T> T* TObjectRef<T>::Get() const noexcept {
    ValidateType();

    UObject* Object{UObjectSystem::Resolve(mHandle)};

    if (Object == nullptr || !Object->GetTypeInfo()->IsA(T::StaticTypeInfo())) {
        return nullptr;
    }

    return static_cast<T*>(Object);
}

template <typename T> T* TObjectRef<T>::operator->() const noexcept {
    return Get();
}

template <typename T> T& TObjectRef<T>::operator*() const noexcept {
    return *Get();
}

template <typename T> TObjectRef<T>::operator bool() const noexcept {
    return IsValid();
}

template <typename T> bool TObjectRef<T>::IsValid() const noexcept {
    return Get() != nullptr;
}

template <typename T> void TObjectRef<T>::Set(T* Object) noexcept {
    ValidateType();

    mHandle = Object != nullptr ? Object->GetHandle() : FObjectHandle{};
}

template <typename T> void TObjectRef<T>::SetHandle(FObjectHandle InHandle) noexcept {
    mHandle = InHandle;
}

template <typename T> void TObjectRef<T>::Reset() noexcept {
    mHandle = {};
}

template <typename T> FObjectHandle TObjectRef<T>::GetHandle() const noexcept {
    return mHandle;
}

template <typename T> constexpr void TObjectRef<T>::ValidateType() noexcept {
    static_assert(std::is_base_of_v<UObject, T>, "TObjectRef<T> requires T to derive from UObject.");
    static_assert(std::is_same_v<typename T::TypeInfoOwner, T>, "TObjectRef<T> requires T to declare its own type information.");
}
