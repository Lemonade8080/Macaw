#pragma once

#include "Core/Common.h"
#include "FGuid.h"
#include "FObjectHandle.h"
#include "TypeInfo.h"

#include <cstddef>
#include <new>

#include "TypeInfo.h"
#include "Core/Base/ErrorHandler.h"

#include "Core/Base/FName.h"
class FArchive;

class UObject;

namespace UObjectSystem {
    FObjectHandle Register(UObject* Object);
    FObjectHandle RegisterWithGuid(UObject* Object, const FGuid& InGuid);
    bool TryGet(Uint32 Index, FObjectHandle& Out);
    Uint32 GetItemCount();
}

class UObject {
public:
    UObject();
    virtual ~UObject() = default;

    UObject(const UObject&) = delete;
    UObject& operator=(const UObject&) = delete;

    const FGuid& GetGuid() const;
    FObjectHandle GetHandle() const;

    FName GetName() const;

    void SetName(FName InName);

    void Save(FArchive& Archive);

    void Load(FArchive& Archive);

    static void* operator new(std::size_t Size);
    static void operator delete(void* Ptr) noexcept;

    static void* operator new(std::size_t Size, std::align_val_t Alignment);
    static void operator delete(void* Ptr, std::align_val_t Alignment) noexcept;

    // RTTI
    JG_DECLARE_ROOT_TYPEINFO(UObject)

    void SetHandle(FObjectHandle InHandle);
    void RestoreGuid(const FGuid& InGuid);

protected:
    virtual void Serialize(FArchive& Archive);

private:
    friend FObjectHandle UObjectSystem::Register(UObject* Object);
    friend FObjectHandle UObjectSystem::RegisterWithGuid(UObject* Object, const FGuid& InGuid);

private:
    FGuid mGuid{};
    FObjectHandle mHandle{};
    FName mName{};
};
