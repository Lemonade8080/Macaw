#include "pch.h"
#include "UObjectSystem.h"

#include "FGuid.h"
#include "FObjectItem.h"
#include "UObject.h"

namespace {
    struct FObjectRegistryState {
        std::vector<FObjectItem> mObjectItems{};
        TMap<FGuid, std::uint32_t> mGuidToIndexMap{};
        std::vector<std::uint32_t> mFreeIndices{};
        std::uint32_t mObjectCount{0};
    };

    FObjectRegistryState& GetRegistryState() {
        static FObjectRegistryState State{};
        return State;
    }
}

FObjectHandle UObjectSystem::Register(UObject* Object) {
    if (Object == nullptr) {
        return {};
    }

    FObjectRegistryState& State{GetRegistryState()};

    std::uint32_t Index{};

    if (!State.mFreeIndices.empty()) {
        Index = State.mFreeIndices.back();
        State.mFreeIndices.pop_back();
    } else {
        Index = static_cast<std::uint32_t>(State.mObjectItems.size());

        State.mObjectItems.emplace_back();
    }

    FObjectItem& Item{State.mObjectItems[Index]};
    Item.mObject = Object;

    const FObjectHandle Handle{ Index, Item.mGeneration};

    Object->SetHandle(Handle);

    ++State.mObjectCount;

    State.mGuidToIndexMap[Object->GetGuid()] = Index;

    return Handle;
}

FObjectHandle UObjectSystem::RegisterWithGuid(UObject* Object, const FGuid& InGuid) {
    Object->RestoreGuid(InGuid);

    return Register(Object);
}

bool UObjectSystem::TryGet(Uint32 Index, FObjectHandle& Out) {
    FObjectRegistryState& State{GetRegistryState()};

    if (Index >= State.mObjectItems.size()) {
        return false;
    }

    FObjectItem& Item{State.mObjectItems[Index]};

    Out = FObjectHandle{ .mIndex = Index, .mGeneration = Item.mGeneration};

    return true;
}

void UObjectSystem::Unregister(UObject* Object, FObjectHandle Handle) {
    if (Object == nullptr) {
        return;
    }

    FObjectRegistryState& State{GetRegistryState()};

    if (Handle.mIndex >= State.mObjectItems.size()) {
        return;
    }

    FObjectItem& Item{State.mObjectItems[Handle.mIndex]};

    if (Item.mObject != Object) {
        return;
    }

    if (Item.mGeneration != Handle.mGeneration) {
        return;
    }

    State.mGuidToIndexMap.erase(Object->GetGuid());

    Item.mObject = nullptr;

    ++Item.mGeneration;

    // Generation 0은 Invalid Handle 용도로 비워 둔다.
    if (Item.mGeneration == 0) {
        ++Item.mGeneration;
    }

    State.mFreeIndices.push_back(Handle.mIndex);

    --State.mObjectCount;
}

UObject* UObjectSystem::Resolve(FObjectHandle Handle) {
    if (!Handle.IsValid()) {
        return nullptr;
    }

    FObjectRegistryState& State{GetRegistryState()};

    if (Handle.mIndex >= State.mObjectItems.size()) {
        return nullptr;
    }

    FObjectItem& Item{State.mObjectItems[Handle.mIndex]};

    if (Item.mObject == nullptr) {
        return nullptr;
    }

    if (Item.mGeneration != Handle.mGeneration) {
        return nullptr;
    }

    return Item.mObject;
}

FObjectHandle UObjectSystem::FindHandleByGuid(const FGuid& Guid) {
    FObjectRegistryState& State{GetRegistryState()};

    auto It{State.mGuidToIndexMap.find(Guid)};
    if (It != State.mGuidToIndexMap.end()) {
        std::uint32_t Index{It->second};

        return FObjectHandle{ Index, State.mObjectItems[Index].mGeneration};
    }

    return {};
}

FObjectHandle UObjectSystem::GetHandle(const UObject* Object) {
    if (Object == nullptr) {
        return {};
    }

    return Object->GetHandle();
}

Uint32 UObjectSystem::GetItemCount() {
    FObjectRegistryState& State{GetRegistryState()};

    return State.mObjectItems.size();
}

std::uint32_t UObjectSystem::GetObjectCount() {
    return GetRegistryState().mObjectCount;
}
