#include "pch.h"
#include "FPropertyPanel.h"
#include "World/UWorld.h"

FPropertyPanel::FPropertyPanel(FWorldEditorContext& InEditorContext, FStateChannel<Uint8>::FReadWriter InGizmoMode, FStateChannel<Uint8>::FReadWriter InGizmoCoordinateSpace, FAssetThumbnailRenderer* InThumbnailRenderer)
    : FEditorWindow("Property Window"),
      mEditorContext(&InEditorContext),
      mGizmoMode(std::move(InGizmoMode)),
      mGizmoCoordinateSpace(std::move(InGizmoCoordinateSpace)) {
    mPropertyEditor.BindThumbnailRenderer(InThumbnailRenderer);
}

void FPropertyPanel::DrawContents() {
    if (mEditorContext == nullptr)
        return;

    AActor* Actor{mEditorContext->GetSelectedActor()};
    if (Actor == nullptr) {
        ImGui::TextDisabled("Select an actor to inspect its components.");
        return;
    }

    DrawGizmoControls();
    DrawComponentList(*Actor);
    ImGui::Separator();

    if (UActorComponent* Component{mEditorContext->GetSelectedComponent()}; Component != nullptr && Component->GetOwner() == Actor) {
        ImGui::Text("Details: %s", GetComponentTypeName(*Component));
        ImGui::PushID(Component);
        UWorld* World{Actor->GetWorld()};
        mPropertyEditor.BindAssetRegistry(World != nullptr ? World->GetAssetRegistry() : nullptr);
        Component->DrawPanels(&mPropertyEditor);
        ImGui::Separator();
        HandleDeleteShortcut(*Actor, *Component);
        ImGui::PopID();
    } else {
        ImGui::TextDisabled("Select a component.");
    }
}

const char* FPropertyPanel::GetComponentTypeName(const UActorComponent& Component) {
    return Component.GetTypeInfo()->mTypeName.data();
}

void FPropertyPanel::DrawGizmoControls() {
    ImGui::TextUnformatted("Gizmo Mode");
    int ModeIndex{static_cast<int>(mGizmoMode.Read())};
    bool BModeChanged{false};
    BModeChanged |= ImGui::RadioButton("Translate", &ModeIndex, static_cast<int>(EGizmoMode::Translate));
    ImGui::SameLine();
    BModeChanged |= ImGui::RadioButton("Rotate", &ModeIndex, static_cast<int>(EGizmoMode::Rotate));
    ImGui::SameLine();
    BModeChanged |= ImGui::RadioButton("Scale", &ModeIndex, static_cast<int>(EGizmoMode::Scale));
    if (BModeChanged)
        mGizmoMode.Emplace(static_cast<Uint8>(ModeIndex));

    ImGui::TextUnformatted("Coordinate Mode");
    int CoordinateSpaceIndex{static_cast<int>(mGizmoCoordinateSpace.Read())};
    bool BCoordinateSpaceChanged{false};
    BCoordinateSpaceChanged |= ImGui::RadioButton("World", &CoordinateSpaceIndex, static_cast<int>(EGizmoCoordinateSpace::World));
    ImGui::SameLine();
    BCoordinateSpaceChanged |= ImGui::RadioButton("Local", &CoordinateSpaceIndex, static_cast<int>(EGizmoCoordinateSpace::Local));
    if (BCoordinateSpaceChanged)
        mGizmoCoordinateSpace.Emplace(static_cast<Uint8>(CoordinateSpaceIndex));
    ImGui::Separator();
}

void FPropertyPanel::DrawComponentList(AActor& Actor) {
    ImGui::TextUnformatted("Components");
    ImGui::SameLine();
    if (ImGui::Button("+ Add Component"))
        ImGui::OpenPopup("AddComponentPopup");

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("Add to selected actor");
        ImGui::Separator();
        if (ImGui::MenuItem("Scene Component"))
            AddSceneComponent<USceneComponent>(Actor);
        if (ImGui::MenuItem("Static Mesh Component"))
            AddSceneComponent<UStaticMeshComponent>(Actor);
        if (ImGui::MenuItem("Camera Component"))
            AddSceneComponent<UCameraComponent>(Actor);
        if (ImGui::MenuItem("Directional Light Component"))
            AddSceneComponent<UDirectionalLightComponent>(Actor);
        if (ImGui::MenuItem("Point Light Component"))
            AddSceneComponent<UPointLightComponent>(Actor);
        if (ImGui::MenuItem("Spot Light Component"))
            AddSceneComponent<USpotLightComponent>(Actor);
        if (ImGui::MenuItem("Box Collider Component"))
            AddSceneComponent<UBoxColliderComponent>(Actor);
        if (ImGui::MenuItem("Billboard Text Component"))
            AddSceneComponent<UBillboardTextComponent>(Actor);
        if (ImGui::MenuItem("Name Tag Component"))
            AddSceneComponent<UNameTagComponent>(Actor);
        ImGui::EndPopup();
    }

    ImGui::BeginChild("ComponentList", ImVec2(0.0f, 180.0f), true);
    for (const std::unique_ptr<UActorComponent>& Component : Actor.GetComponents()) {
        auto* SceneComponent{static_cast<USceneComponent*>(Component.get())};

        if (SceneComponent == nullptr) {
            DrawComponentNode(*Component, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        } else if (SceneComponent->GetParent() == nullptr || SceneComponent->GetParent()->GetOwner() != &Actor) {
            DrawSceneComponentTree(Actor, *SceneComponent);
        }
    }
    ImGui::EndChild();
}

void FPropertyPanel::DrawSceneComponentTree(AActor& Actor, USceneComponent& Component) {
    const bool BHasOwnedChildren{std::ranges::any_of(Component.GetChildren(), [&Actor](const TObjectRef<USceneComponent>& Child) {
        return Child.Get() != nullptr && Child.Get()->GetOwner() == &Actor;
    })};
    ImGuiTreeNodeFlags Flags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth};
    if (!BHasOwnedChildren)
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (mEditorContext->GetSelectedComponent() == &Component)
        Flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(&Component);
    const bool BOpen{ImGui::TreeNodeEx("Component", Flags, "%s", GetComponentTypeName(Component))};
    if (ImGui::IsItemClicked())
        mEditorContext->SetSelectedComponent(&Component);
    if (BHasOwnedChildren && BOpen) {
        for (const TObjectRef<USceneComponent>& Child : Component.GetChildren()) {
            if (USceneComponent* ChildComponent{Child.Get()}; ChildComponent != nullptr && ChildComponent->GetOwner() == &Actor) {
                DrawSceneComponentTree(Actor, *ChildComponent);
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void FPropertyPanel::DrawComponentNode(UActorComponent& Component, ImGuiTreeNodeFlags Flags) {
    if (mEditorContext->GetSelectedComponent() == &Component)
        Flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::PushID(&Component);
    ImGui::TreeNodeEx("Component", Flags, "%s", GetComponentTypeName(Component));
    if (ImGui::IsItemClicked())
        mEditorContext->SetSelectedComponent(&Component);
    ImGui::PopID();
}

void FPropertyPanel::HandleDeleteShortcut(AActor& Actor, UActorComponent& Component) {
    const ImGuiIO& IO{ImGui::GetIO()};
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || IO.WantTextInput || ImGui::IsAnyItemActive() || !ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        return;
    }

    Component.DestroyComponent();
    mEditorContext->SetSelectedActor(&Actor);
}
