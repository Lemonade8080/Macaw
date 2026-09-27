#include "pch.h"
#include "Memory.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

namespace {
    struct FAllocationHeader {
        void* mRawPointer{nullptr};
        std::size_t mSize{0};
        std::size_t mAlignment{0};
        Memory::EMemoryTag mTag{Memory::EMemoryTag::Unknown};
    };

    FString Str{};
}

const char* Memory::GetMemoryTagName(EMemoryTag Tag) {
    return Stat::GetMemoryTagName(Tag);
}

void* Memory::Allocate(std::size_t Size, std::size_t Alignment, EMemoryTag Tag) {
    if (Alignment == 0 || (Alignment & (Alignment - 1)) != 0) {
        throw std::invalid_argument("Invalid memory alignment");
    }

    const std::size_t EffectiveAlignment{(std::max)(Alignment, alignof(FAllocationHeader))};
    const std::size_t PayloadSize{Size == 0 ? 1 : Size};
    const std::size_t MaxSize{std::numeric_limits<std::size_t>::max()};

    if (EffectiveAlignment - 1 > MaxSize - sizeof(FAllocationHeader)) {
        throw std::bad_alloc();
    }

    const std::size_t Overhead{sizeof(FAllocationHeader) + (EffectiveAlignment - 1)};

    if (PayloadSize > MaxSize - Overhead) {
        throw std::bad_alloc();
    }

    const std::size_t TotalSize{Overhead + PayloadSize};
    void* RawPointer{::operator new(TotalSize)};
    void* UserPointer{static_cast<std::byte*>(RawPointer) + sizeof(FAllocationHeader)};

    std::size_t Space{TotalSize - sizeof(FAllocationHeader)};

    if (std::align(EffectiveAlignment, PayloadSize, UserPointer, Space) == nullptr) {
        ::operator delete(RawPointer);
        throw std::bad_alloc();
    }

    void* HeaderAddress{static_cast<std::byte*>(UserPointer) - sizeof(FAllocationHeader)};

    ::new (HeaderAddress) FAllocationHeader{ RawPointer, Size, Alignment, Tag};

    Stat::RecordAllocation(Size, Tag);

    return UserPointer;
}

void Memory::Free(void* Ptr) noexcept {
    if (Ptr == nullptr) {
        return;
    }

    auto* Header{reinterpret_cast<FAllocationHeader*>(static_cast<std::byte*>(Ptr) - sizeof(FAllocationHeader))};

    void* RawPointer{Header->mRawPointer};
    const std::size_t Size{Header->mSize};
    const EMemoryTag Tag{Header->mTag};

    Stat::RecordDeallocation(Size, Tag);

    Header->~FAllocationHeader();
    ::operator delete(RawPointer);
}

Memory::FMemoryStats Memory::GetStats() {
    return Stat::GetMemoryStats();
}
