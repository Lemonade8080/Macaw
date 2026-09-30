#include "pch.h"

#include "URenderSubsystem.h"

#include "World/AActor.h"

URenderSubsystem::URenderSubsystem()
    : mSceneId{AllocateRenderSceneId()} {
}

void URenderSubsystem::RegisterComponent(UStaticMeshComponent* Component) {
    if (Component == nullptr || !Component->GetHandle().IsValid()) {
        return;
    }

    const FObjectHandle Handle{Component->GetHandle()};

    if (!mComponentIndices.emplace(GetComponentKey(Handle), mComponents.size()).second) {
        return;
    }

    mComponents.push_back(Component);

    MarkComponentDirty(Handle);
}

void URenderSubsystem::UnregisterComponent(UStaticMeshComponent* Component) {
    if (Component == nullptr) {
        return;
    }

    const FObjectHandle Handle{Component->GetHandle()};
    const auto Position{mComponentIndices.find(GetComponentKey(Handle))};

    if (Position == mComponentIndices.end() || mComponents[Position->second] != Component) {
        return;
    }

    const std::size_t Index{Position->second};
    if (Index + 1 != mComponents.size()) {
        mComponents[Index] = mComponents.back();
        mComponentIndices[GetComponentKey(mComponents[Index]->GetHandle())] = Index;
    }

    mComponents.pop_back();
    mComponentIndices.erase(Position);

    MarkComponentDirty(Handle);
}

void URenderSubsystem::UpdateComponentRenderState(UStaticMeshComponent* Component) {
    if (ContainsComponent(Component)) {
        MarkComponentDirty(Component->GetHandle());
    }
}

void URenderSubsystem::BuildRenderProbes(FSceneRenderData& Scene) {
    Scene.mSceneId = mSceneId;

    Scene.mObjectUpdates.clear();
    Scene.mObjectUpdates.reserve(mDirtyComponents.size());

    for (const FObjectHandle Handle : mDirtyComponents) {
        FRenderObjectUpdate Update{};
        Update.mComponentHandle = Handle;

        const auto Position{mComponentIndices.find(GetComponentKey(Handle))};
        const UStaticMeshComponent* Component{Position != mComponentIndices.end() ? mComponents[Position->second] : nullptr};
        Update.mRemoved = Component == nullptr || !Component->IsActive() || !Component->IsVisible();

        if (!Update.mRemoved) {
            Component->MakeRender(Update.mProbe);
            Update.mProbe.mOwnerHandle = Component->GetOwner()->GetHandle();
        }

        Scene.mObjectUpdates.push_back(Update);
    }

    if (!Scene.mObjectUpdates.empty()) {
        ++mRevision;
    }

    Scene.mRevision = mRevision;

    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();
}

bool URenderSubsystem::ContainsComponent(const UStaticMeshComponent* Component) const {
    if (Component == nullptr) {
        return false;
    }

    const auto Position{mComponentIndices.find(GetComponentKey(Component->GetHandle()))};
    return Position != mComponentIndices.end() && mComponents[Position->second] == Component;
}

const TArray<UStaticMeshComponent*>& URenderSubsystem::GetRegisteredComponents() const {
    return mComponents;
}

Uint64 URenderSubsystem::GetSceneId() const {
    return mSceneId;
}

Uint64 URenderSubsystem::GetComponentKey(FObjectHandle Handle) {
    return static_cast<Uint64>(Handle.mGeneration) << 32 | Handle.mIndex;
}

void URenderSubsystem::MarkComponentDirty(FObjectHandle Handle) {
    if (mDirtyComponentKeys.insert(GetComponentKey(Handle)).second) {
        mDirtyComponents.push_back(Handle);
    }
}

void URenderSubsystem::OnDeinitialize() {
    mComponents.clear();
    mComponentIndices.clear();

    mDirtyComponents.clear();
    mDirtyComponentKeys.clear();

    mSceneId = AllocateRenderSceneId();
    mRevision = {};
}
