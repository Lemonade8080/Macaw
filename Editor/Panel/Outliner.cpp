#include "pch.h"

#include "Outliner.h"
#include "World/Component/UNameTagComponent.h"
#include "World/Component/UMeshComponent.h"
#include "World/FWorldEditorContext.h"

#include <algorithm>
#include <ranges>

namespace {
    constexpr const char* ActorDragDropPayloadType{"OUTLINER_ACTOR"};
}

FOutlinerPanel::FOutlinerPanel(UWorld& InWorld, FWorldEditorContext& InEditorContext)
    : FEditorWindow("Outliner###OutlinerPanel"),
      mWorld(&InWorld),
      mEditorContext(&InEditorContext) {
}

void FOutlinerPanel::DrawContents() {
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##ActorFilter", "Search", mActorFilter.InputBuf, IM_ARRAYSIZE(mActorFilter.InputBuf))) {
        mActorFilter.Build();
    }

    const ImGuiTableFlags TableFlags{ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY};
    if (ImGui::BeginTable("OutlinerActorList", 2, TableFlags, ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()))) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.70f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableHeadersRow();
        DrawRootActors();
        DrawRootActorDropTarget();
        ImGui::EndTable();
    }

    HandleDeleteShortcut();
    ImGui::TextDisabled("%zu Actors", mWorld->GetActors().size());
}

void FOutlinerPanel::PushWindowStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.080f, 0.095f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.165f, 0.215f, 0.285f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.215f, 0.310f, 0.425f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.255f, 0.385f, 0.540f, 1.0f));
}

void FOutlinerPanel::PopWindowStyle() {
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

bool FOutlinerPanel::MatchesActor(const AActor& Actor) const {
    const FString Label{Actor.GetGuid().ToString()};
    const std::string_view TypeName{Actor.GetTypeInfo()->mTypeName};
    if (mActorFilter.PassFilter(Label.c_str()) || mActorFilter.PassFilter(TypeName.data(), TypeName.data() + TypeName.size())) {
        return true;
    }

    return std::ranges::any_of(mWorld->GetActors(), [this, &Actor](const std::unique_ptr<AActor>& ChildActor) {
        return IsActorAttachedTo(*ChildActor, Actor) && MatchesActor(*ChildActor);
    });
}

bool FOutlinerPanel::IsActorAttachedTo(const AActor& Actor, const AActor& ParentActor) const {
    const USceneComponent* RootComponent{Actor.GetRootComponent()};
    const USceneComponent* ParentComponent{RootComponent != nullptr ? RootComponent->GetParent() : nullptr};
    return ParentComponent != nullptr && ParentComponent->GetOwner() == &ParentActor && &Actor != &ParentActor;
}

bool FOutlinerPanel::IsRootActor(const AActor& Actor) const {
    const USceneComponent* RootComponent{Actor.GetRootComponent()};
    const USceneComponent* ParentComponent{RootComponent != nullptr ? RootComponent->GetParent() : nullptr};
    const AActor* ParentActor{ParentComponent != nullptr ? ParentComponent->GetOwner() : nullptr};
    return ParentActor == nullptr || ParentActor == &Actor;
}

bool FOutlinerPanel::HasActorChildren(const AActor& Actor) const {
    return std::ranges::any_of(mWorld->GetActors(), [this, &Actor](const std::unique_ptr<AActor>& ChildActor) {
        return IsActorAttachedTo(*ChildActor, Actor);
    });
}

void FOutlinerPanel::DrawActorDragSource(AActor& Actor) {
    if (ImGui::BeginDragDropSource()) {
        AActor* DraggedActor{&Actor};
        ImGui::SetDragDropPayload(ActorDragDropPayloadType, &DraggedActor, sizeof(DraggedActor));
        ImGui::TextUnformatted(Actor.GetName().ToString().c_str());
        ImGui::EndDragDropSource();
    }
}

void FOutlinerPanel::AcceptActorChildDrop(AActor& ParentActor) {
    if (!ImGui::BeginDragDropTarget()) {
        return;
    }

    if (const ImGuiPayload* Payload{ImGui::AcceptDragDropPayload(ActorDragDropPayloadType)};
        Payload != nullptr && Payload->Delivery && Payload->DataSize == sizeof(AActor*)) {
        AActor* DraggedActor{*static_cast<AActor* const*>(Payload->Data)};
        USceneComponent* DraggedRoot{DraggedActor != nullptr ? DraggedActor->GetRootComponent() : nullptr};
        USceneComponent* ParentRoot{ParentActor.GetRootComponent()};

        if (DraggedActor != nullptr && DraggedActor != &ParentActor && DraggedActor->GetWorld() == mWorld &&
            ParentActor.GetWorld() == mWorld && DraggedRoot != nullptr && ParentRoot != nullptr) {
            DraggedRoot->AttachToComponent(ParentRoot, EAttachmentTransformRule::KeepWorldTransform);
        }
    }

    ImGui::EndDragDropTarget();
}

void FOutlinerPanel::DrawRootActorDropTarget() {
    const float AvailableHeight{std::max(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y)};
    ImGui::TableNextRow(ImGuiTableRowFlags_None, AvailableHeight);
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID("OutlinerRootActorDropTarget");
    ImGui::Selectable("##RootActorDropTarget", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, AvailableHeight));

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* Payload{ImGui::AcceptDragDropPayload(ActorDragDropPayloadType)};
            Payload != nullptr && Payload->Delivery && Payload->DataSize == sizeof(AActor*)) {
            AActor* DraggedActor{*static_cast<AActor* const*>(Payload->Data)};
            USceneComponent* DraggedRoot{DraggedActor != nullptr ? DraggedActor->GetRootComponent() : nullptr};

            if (DraggedActor != nullptr && DraggedActor->GetWorld() == mWorld && DraggedRoot != nullptr) {
                DraggedRoot->DetachFromComponent(EAttachmentTransformRule::KeepWorldTransform);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
}

void FOutlinerPanel::HandleDeleteShortcut() {
    const ImGuiIO& IO{ImGui::GetIO()};
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || IO.WantTextInput || ImGui::IsAnyItemActive() || !ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        return;
    }

    AActor* Actor{mEditorContext->GetSelectedActor()};
    if (Actor != nullptr && mWorld->DestroyActor(Actor)) {
        mWorld->FlushPendingDestroyActors();
    }
}

void FOutlinerPanel::DrawActor(AActor& Actor) {
    if (!MatchesActor(Actor)) {
        return;
    }

    const FString Label{Actor.GetName().ToString()};
    const std::string_view TypeName{Actor.GetTypeInfo()->mTypeName};
    const bool BHasChildren{HasActorChildren(Actor)};
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(Label.c_str());

    ImGuiTreeNodeFlags Flags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAllColumns};
    if (!BHasChildren) {
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (mEditorContext->GetSelectedActor() == &Actor) {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool BOpen{ImGui::TreeNodeEx("Actor", Flags, "%s", Label.c_str())};
    if (ImGui::IsItemClicked()) {
        auto Prev{mEditorContext->GetSelectedActor()};

        if (Prev != nullptr) {
            if (UNameTagComponent * NameTag{Prev->GetComponent<UNameTagComponent>()}) {
                NameTag->SetActive(false);
            }
        }

        mEditorContext->SetSelectedActor(&Actor);

        if (UNameTagComponent * NameTag{Actor.GetComponent<UNameTagComponent>()}) {
            NameTag->SetActive(true);
        }
    }

    DrawActorDragSource(Actor);
    AcceptActorChildDrop(Actor);

    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("%.*s", static_cast<int>(TypeName.size()), TypeName.data());

    if (BHasChildren && BOpen) {
        for (const std::unique_ptr<AActor>& ChildActor : mWorld->GetActors()) {
            if (IsActorAttachedTo(*ChildActor, Actor)) {
                DrawActor(*ChildActor);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void FOutlinerPanel::DrawRootActors() {
    for (const std::unique_ptr<AActor>& Actor : mWorld->GetActors()) {
        if (IsRootActor(*Actor)) {
            DrawActor(*Actor);
        }
    }
}
