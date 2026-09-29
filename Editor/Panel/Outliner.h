#pragma once

#include "ImGui/imgui.h"
#include "Editor/Panel/FEditorWindow.h"
#include "World/UWorld.h"
#include "World/FWorldEditorContext.h"

class FOutlinerPanel : public FEditorWindow {
public:
    FOutlinerPanel(UWorld& InWorld, FWorldEditorContext& InEditorContext);
    void MarkDirty(){ bHierarchyDirty = true;}
    void RebuildHierarchy();

private:
    void DrawContents() override;
    void PushWindowStyle() override;
    void PopWindowStyle() override;

    bool MatchesActor(const AActor& Actor) const;
    bool IsActorAttachedTo(const AActor& Actor, const AActor& ParentActor) const;
    bool IsRootActor(const AActor& Actor) const;
    bool HasActorChildren(const AActor& Actor) const;
    void DrawActorDragSource(AActor& Actor);
    void AcceptActorChildDrop(AActor& ParentActor);
    void DrawRootActorDropTarget();
    void HandleDeleteShortcut();
    void DrawActor(AActor& Actor);
    void DrawRootActors();

    UWorld* mWorld{};
    FWorldEditorContext* mEditorContext{};
    ImGuiTextFilter mActorFilter{};

    TMap<AActor*, TArray<AActor*>> mChildrenByParent;

    uint64 mCachedRevision = 0;
    bool bHierarchyDirty = true;
};
