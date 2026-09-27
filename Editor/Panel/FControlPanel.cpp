#include "pch.h"
#include "FControlPanel.h"

#include "Editor/FileDialog.h"

#include <windows.h>
#include <filesystem>
#include <map>

#include "Editor/Settings/FEditorConfigManager.h"
#include "../../Core/Base/TypeRegistry.h"
#include "World/AActor.h"
#include "World/Component/UActorComponent.h"
#include "World/Component/UStaticMeshComponent.h"
#include "World/UWorld.h"
#include "Asset/Pipeline/UPipeline.h"

#include "../../Core/Console/Console.h"
#include "Core/Base/TObjectIterator.h"

void FControlPanel::DrawPanel() {
    const char* PrimitiveMeshTypes[]{ "/Game/System/Mesh/Cube.bin", "/Game/System/Mesh/Sphere.bin", "/Game/System/Mesh/Plane.bin", "/Game/System/Mesh/Cylinder.bin", "/Game/System/Mesh/Capsule.bin", "/Game/System/Mesh/Cone.bin", "/Game/System/Mesh/Torus.bin", "/Game/System/Mesh/Pyramid.bin"};

    if (ImGui::BeginMenu("Create")) {
        std::vector<const FTypeInfo*> SpawnableComponentTypes{};
        for (const FTypeInfo* Type : TypeRegistry::GetRegisteredTypes()) {
            if (Type != nullptr && Type->mCreator != nullptr &&
                Type->IsA(UActorComponent::StaticTypeInfo())) {
                SpawnableComponentTypes.push_back(Type);
            }
        }

        ImGui::TextDisabled("Spawn Component");
        if (SpawnableComponentTypes.empty()) {
            ImGui::TextDisabled("No spawnable component types are registered.");
        } else {
            if (mSelectedComponentIndex < 0) {
                const auto StaticMeshType{std::ranges::find_if(SpawnableComponentTypes, [](const FTypeInfo* Type) {
                    return Type->IsA(UStaticMeshComponent::StaticTypeInfo());
                })};

                mSelectedComponentIndex = StaticMeshType != SpawnableComponentTypes.end() ? static_cast<int>(std::distance(SpawnableComponentTypes.begin(), StaticMeshType)) : 0;
            }
            mSelectedComponentIndex = std::clamp(mSelectedComponentIndex, 0, static_cast<int>(SpawnableComponentTypes.size()) - 1);
            const FTypeInfo* SelectedComponentType{SpawnableComponentTypes[mSelectedComponentIndex]};

            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo("Component", SelectedComponentType->mTypeName.data())) {
                for (int Index{0}; Index < static_cast<int>(SpawnableComponentTypes.size()); ++Index) {
                    const bool BIsSelected{Index == mSelectedComponentIndex};
                    if (ImGui::Selectable(SpawnableComponentTypes[Index]->mTypeName.data(), BIsSelected)) {
                        mSelectedComponentIndex = Index;
                        SelectedComponentType = SpawnableComponentTypes[Index];
                    }

                    if (BIsSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const bool BIsStaticMesh{SelectedComponentType->IsA(UStaticMeshComponent::StaticTypeInfo())};
            if (BIsStaticMesh) {
                ImGui::SetNextItemWidth(180.0f);
                ImGui::Combo("Mesh", &mSelectedMeshIndex, PrimitiveMeshTypes, IM_ARRAYSIZE(PrimitiveMeshTypes));
            }

            ImGui::InputInt("Number of Objects to Spawn", &mSpawnCountToRequest);
            if (mSpawnCountToRequest < 1) {
                mSpawnCountToRequest = 1;
            }

            if (ImGui::Button("Spawn Object(s)")) {
                mEditorToWorldSender.TryEmplace<FMessageSpawnComponent>(FString{SelectedComponentType->mTypeName.data()}, FString{BIsStaticMesh ? PrimitiveMeshTypes[mSelectedMeshIndex] : ""}, static_cast<Uint32>(mSpawnCountToRequest));
            }
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("Scene Name", mSceneNameBuffer, IM_ARRAYSIZE(mSceneNameBuffer));

        if (ImGui::Button("Save Scene")) {
            mEditorToWorldSender.TryEmplace<FMessageSaveScene>(FString{mSceneNameBuffer});
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            const FString FilePath{OpenFileDialog(mWindowHandle, "./scenes", "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0", "json")};

            if (!FilePath.empty()) {
                mEditorToWorldSender.TryEmplace<FMessageLoadScene>(FString{FilePath});
            }

            std::size_t SlashPos{FilePath.find_last_of("\\/")};
            std::string FileName{};
            if (SlashPos != std::string::npos)
                FileName = FilePath.substr(SlashPos + 1);
            else
                FileName = FilePath;

            std::size_t DotPos{FileName.find_last_of('.')};
            if (DotPos != std::string::npos)
                FileName = FileName.substr(0, DotPos);

            if (FileName.size() < sizeof(mSceneNameBuffer))
                std::memcpy(mSceneNameBuffer, FileName.data(), FileName.size() + 1);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Components")) {
        UWorld* World{mEditorContext != nullptr ? mEditorContext->GetWorld() : nullptr};
        if (World == nullptr) {
            ImGui::TextDisabled("World is unavailable.");
        } else {
            struct FComponentTypeState {
                std::size_t mActiveCount{0};
                std::vector<UActorComponent*> mComponents{};
            };

            std::map<FString, FComponentTypeState> ComponentsByType{};
            for (UActorComponent& Component : UObjectSystem::Objects<UActorComponent>()) {
                AActor* Owner{Component.GetOwner()};
                if (Owner == nullptr || Owner->GetWorld() != World) {
                    continue;
                }

                FComponentTypeState& TypeState{ComponentsByType[FString{Component.GetTypeInfo()->mTypeName.data()}]};

                TypeState.mComponents.push_back(&Component);
                TypeState.mActiveCount += Component.IsActive() ? 1 : 0;
            }

            mComponentFilter.Draw("Search types##SceneComponents", 240.0f);
            const auto IsTypeVisible{[this](const FString& TypeName) {
                return mComponentFilter.PassFilter(TypeName.c_str());
            }};
            const auto SetVisibleTypesActive{[&ComponentsByType, &IsTypeVisible](bool BActive) {
                for (auto& [TypeName, TypeState] : ComponentsByType) {
                    if (!IsTypeVisible(TypeName)) {
                        continue;
                    }

                    for (UActorComponent* Component : TypeState.mComponents) {
                        if (Component != nullptr) {
                            Component->SetActive(BActive);
                        }
                    }
                }
            }};

            if (ImGui::Button("Enable filtered")) {
                SetVisibleTypesActive(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Disable filtered")) {
                SetVisibleTypesActive(false);
            }
            ImGui::Separator();

            bool BHasVisibleType{false};
            for (const auto& [TypeName, TypeState] : ComponentsByType) {
                if (!IsTypeVisible(TypeName)) {
                    continue;
                }

                BHasVisibleType = true;
                ImGui::PushID(TypeName.c_str());
                const std::size_t ComponentCount{TypeState.mComponents.size()};
                const bool BAllActive{TypeState.mActiveCount == ComponentCount};
                const bool BMixed{TypeState.mActiveCount != 0 && !BAllActive};
                bool BTypeActive{BAllActive};
                ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, BMixed);
                FString Label{TypeName};
                Label += " (";
                Label += std::to_string(TypeState.mActiveCount).c_str();
                Label += "/";
                Label += std::to_string(ComponentCount).c_str();
                Label += ")";
                const bool BTypeChanged{ImGui::Checkbox(Label.c_str(), &BTypeActive)};
                ImGui::PopItemFlag();
                if (BTypeChanged) {
                    for (UActorComponent* Component : TypeState.mComponents) {
                        if (Component != nullptr) {
                            Component->SetActive(BTypeActive);
                        }
                    }
                }
                ImGui::PopID();
            }

            if (!BHasVisibleType) {
                ImGui::TextDisabled("No component types match the current filter.");
            }
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("EditorSettings")) {
        if (ImGui::BeginMenu("Camera")) {
            const FEditorSettings Settings{mEditorContext->GetEditorSettings()};
            float MoveSensitivity{Settings.mMoveSensitivity};
            if (ImGui::SliderFloat("MoveSensitivity", &MoveSensitivity, 1.f, 100.0f)) {
                mEditorContext->SetMoveSensitivity(MoveSensitivity);
            }

            float RotationSensitivity{Settings.mRotationSensitivity};
            if (ImGui::SliderFloat("RotationSensitivity", &RotationSensitivity, 0.1f, 5.0f)) {
                mEditorContext->SetRotationSensitivity(RotationSensitivity);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Grid")) {
            const FEditorSettings Settings{mEditorContext->GetEditorSettings()};
            float GridSize{Settings.mGridSize};
            if (ImGui::SliderFloat("GridSize", &GridSize, 0.1f, 100.0f)) {
                mEditorContext->SetGridSize(GridSize);
            }
            bool GridSnapEnabled{Settings.mGridSnapEnabled};
            if (ImGui::Checkbox("Snap to Grid", &GridSnapEnabled)) {
                mEditorContext->SetGridSnapEnabled(GridSnapEnabled);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ON/OFF")) {
            const FEditorSettings Settings{mEditorContext->GetEditorSettings()};
            bool GridVisible{Settings.mGridVisible};
            bool AxisVisible{Settings.mAxisVisible};

            if (ImGui::Checkbox("Grid", &GridVisible)) {
                mEditorContext->SetGridVisible(GridVisible);
            }

            if (ImGui::Checkbox("World Axis", &AxisVisible)) {
                mEditorContext->SetAxisVisible(AxisVisible);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();
    const ERenderMode RenderModeValues[]{ERenderMode::Lit, ERenderMode::Unlit, ERenderMode::Wireframe};
    int RenderIndex{0};
    for (int Index{0}; Index < IM_ARRAYSIZE(RenderModeValues); ++Index) {
        if (mEditorContext->GetRenderModeState() == static_cast<std::size_t>(RenderModeValues[Index])) {
            RenderIndex = Index;
            break;
        }
    }
    const char* RenderModes[]{"Lit", "Unlit", "Wireframe"};
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Render Mode", &RenderIndex, RenderModes, IM_ARRAYSIZE(RenderModes))) {
        mEditorContext->SetRenderModeState(static_cast<std::size_t>(RenderModeValues[RenderIndex]));
    }

    if (ImGui::Button("Import")) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Import Button Click");

        const FString FilePath{OpenFileDialog(mWindowHandle, "./Content/ModelingFiles", "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0", "obj")};

        mEditorToWorldSender.TryEmplace<FMessageImportMesh>(FString{"ObjImport"}, FString{FilePath}, FString{"./Content/Metadata/MonkeyMesh.meta"});
    }

}

FControlPanel::FControlPanel(FWorldEditorContext& InEditorContext, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender)
	: mEditorContext{&InEditorContext},
	  mEditorToWorldSender{std::move(InEditorToWorldSender)},
	  mWindowHandle{InputWindowHandle} {
}
