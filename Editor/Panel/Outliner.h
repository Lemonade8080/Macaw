#pragma once

#include "ImGui/imgui.h"
#include "Editor/Panel/FEditorWindow.h"
#include "World/UWorld.h"
#include "World/FWorldEditorContext.h"

struct FOutlinerVisibleItem
{
    AActor* Actor = nullptr;
    uint32 Depth = 0;
};

class FOutlinerPanel : public FEditorWindow {
public:
    FOutlinerPanel(UWorld& InWorld, FWorldEditorContext& InEditorContext);
    void MarkDirty(){ bHierarchyDirty = true;}

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

    /* Outliner */

    // 계층 확인용 캐시
    TMap<AActor*, TArray<AActor*>> mChildrenByParent;
    TArray<FOutlinerVisibleItem> mVisibleItems;
    TSet<AActor*> mExpandedActors;

    void RebuildHierarchy();
    void RebuildVisibleItems();
    void AddVisibleActor(AActor* ParentActor, uint32 Depth);

    uint64 mCachedRevision = 0;
    bool bHierarchyDirty = true;
    bool bVisibleDirty = true;
};

