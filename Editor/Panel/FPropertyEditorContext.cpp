#include "pch.h"
#include "FPropertyEditorContext.h"

#include "ImGui/imgui.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UAsset.h"
#include "Core/Base/TypeInfo.h"

#include "Asset/UTexture.h"
#include "Asset/UMesh.h"
#include "Asset/UMaterial.h"
#include "Editor/View/FAssetThumbnailRenderer.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
    constexpr char StaticMeshAssetPayloadType[]{"MACAW_STATIC_MESH_ASSET"};
    constexpr char MaterialAssetPayloadType[]{"MACAW_MATERIAL_ASSET"};
    constexpr char TextureAssetPayloadType[]{"MACAW_TEXTURE_ASSET"};

    void ApplyAssetBrowserDrop(const IAssetRegistry* Registry, const FTypeInfo& AssetType, FAssetHandle CurrentHandle, const std::function<void(FAssetHandle)>& Setter) {
        const char* PayloadType{};
        if (AssetType.IsA(UMesh::StaticTypeInfo())) {
            PayloadType = StaticMeshAssetPayloadType;
        } else if (AssetType.IsA(UMaterial::StaticTypeInfo())) {
            PayloadType = MaterialAssetPayloadType;
        } else if (AssetType.IsA(UTexture::StaticTypeInfo())) {
            PayloadType = TextureAssetPayloadType;
        }

        if (PayloadType == nullptr || !ImGui::BeginDragDropTarget()) {
            return;
        }

        const ImGuiPayload* Payload{ImGui::AcceptDragDropPayload(PayloadType)};
        if (Payload != nullptr && Payload->DataSize == sizeof(FAssetHandle)) {
            FAssetHandle DroppedHandle{};
            std::memcpy(&DroppedHandle, Payload->Data, sizeof(DroppedHandle));
            const UAsset* DroppedAsset{Registry->ResolveAsset<UAsset>(DroppedHandle)};
            if (DroppedAsset != nullptr && DroppedAsset->GetTypeInfo()->IsA(&AssetType) && DroppedHandle != CurrentHandle) {
                Setter(DroppedHandle);
            }
        }

        ImGui::EndDragDropTarget();
    }
}

bool FPropertyEditorContext::BeginCategory(const char* Label, bool BDefaultOpen) const {
    return ImGui::CollapsingHeader(Label, BDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void FPropertyEditorContext::DrawDisabledText(const char* Text) const {
    ImGui::TextDisabled("%s", Text);
}

void FPropertyEditorContext::DrawButton(const char* Label, const std::function<void()>& OnClicked) const {
    if (ImGui::Button(Label)) {
        OnClicked();
    }
}

void FPropertyEditorContext::DrawBool(const char* Label, bool Value, const std::function<void(bool)>& Setter) const {
    if (ImGui::Checkbox(Label, &Value)) {
        Setter(Value);
    }
}

void FPropertyEditorContext::DrawFloat(const char* Label, float Value, float Speed, float Min, float Max, const std::function<void(float)>& Setter) const {
    if (ImGui::DragFloat(Label, &Value, Speed, Min, Max)) {
        Setter(Value);
    }
}

void FPropertyEditorContext::DrawVector2(const char* Label, const FVector2& Value, float Speed, float Min, float Max, const std::function<void(const FVector2&)>& Setter) const {
    FVector2 EditedValue{Value};
    if (ImGui::DragFloat2(Label, &EditedValue.mX, Speed, Min, Max)) {
        Setter(EditedValue);
    }
}

void FPropertyEditorContext::DrawVector3(const char* Label, const FVector3& Value, float Speed, float Min, float Max, const std::function<void(const FVector3&)>& Setter) const {
    FVector3 EditedValue{Value};
    if (ImGui::DragFloat3(Label, &EditedValue.mX, Speed, Min, Max)) {
        Setter(EditedValue);
    }
}

void FPropertyEditorContext::DrawColor(const char* Label, const FVector4& Value, const std::function<void(const FVector4&)>& Setter) const {
    FVector4 EditedValue{Value};
    if (ImGui::ColorEdit4(Label, &EditedValue.mX)) {
        Setter(EditedValue);
    }
}

void FPropertyEditorContext::DrawText(const char* Label, const FString& Value, const std::function<void(const FString&)>& Setter) const {
    std::array<char, 2048> TextBuffer{};
    const std::size_t CopyLength{std::min(Value.size(), TextBuffer.size() - 1)};
    std::memcpy(TextBuffer.data(), Value.data(), CopyLength);
    if (ImGui::InputTextMultiline(Label, TextBuffer.data(), TextBuffer.size(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5.0f))) {
        Setter(TextBuffer.data());
    }
}

void FPropertyEditorContext::DrawTransform(const char* Label, const FTransform& Value, const std::function<void(const FTransform&)>& Setter) {
    const ImGuiID TransformId{ImGui::GetID(Label)};
    if (mEditingTransformId != TransformId) {
        mEditingTransformId = TransformId;
    }

    UpdateTransformFields(Value);

    bool BChanged{false};
    BChanged |= ImGui::DragFloat3("Position", &mEditPosition.mX, 0.1f);
    BChanged |= ImGui::DragFloat3("Rotation", &mEditRotation.mX, 0.5f);
    BChanged |= ImGui::DragFloat3("Scale", &mEditScale.mX, 0.05f, 0.001f, FLT_MAX);
    BChanged |= ImGui::Checkbox("Absolute Location", &mBAbsoluteLocation);
    BChanged |= ImGui::Checkbox("Absolute Rotation", &mBAbsoluteRotation);
    BChanged |= ImGui::Checkbox("Absolute Scale", &mBAbsoluteScale);

    if (BChanged) {
        mEditScale = FVector::Max(mEditScale, FVector{0.001f, 0.001f, 0.001f});
        Setter(BuildDesiredTransform());
    }
}

void FPropertyEditorContext::DrawReferencePicker(const char* Label, const char* Preview, bool BNoneSelected, const std::function<void()>& ClearSelection, const std::vector<FPropertyReferenceOption>& Options) const {
    if (!ImGui::BeginCombo(Label, Preview)) {
        return;
    }
    if (ImGui::Selectable("None", BNoneSelected)) {
        ClearSelection();
    }
    for (const FPropertyReferenceOption& Option : Options) {
        ImGui::PushID(Option.mId);
        if (ImGui::Selectable(Option.mLabel.c_str(), Option.mBSelected)) {
            Option.mOnSelected();
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
}

void FPropertyEditorContext::DrawAssetPicker(const char* Label, const FTypeInfo& AssetType, FAssetHandle CurrentHandle, const std::function<void(FAssetHandle)>& Setter) const {
    if (mAssetRegistry == nullptr) {
        ImGui::TextDisabled("%s: Asset registry unavailable", Label);
        return;
    }

    const IAssetRegistry* Registry{mAssetRegistry};
    const UAsset* Current{Registry->ResolveAsset<UAsset>(CurrentHandle)};

    if (Current != nullptr && !Current->GetTypeInfo()->IsA(&AssetType)) {
        Current = nullptr;
    }

    const FString PreviewName{Current != nullptr ? Current->GetAssetName() : FString{"None"}};
    const bool BSupportsThumbnail{SupportsAssetThumbnail(AssetType)};

    ImGui::PushID(Label);

    if (BSupportsThumbnail) {
        ID3D11ShaderResourceView* Thumbnail{GetAssetThumbnail(Registry, CurrentHandle)};

        if (Thumbnail != nullptr) {
            ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(Thumbnail)), ImVec2(48.0f, 48.0f));
        } else {
            ImGui::Button("##AssetThumbnail", ImVec2(48.0f, 48.0f));
        }

        ApplyAssetBrowserDrop(Registry, AssetType, CurrentHandle, Setter);
        ImGui::SameLine();
    }

    if (ImGui::BeginCombo(Label, PreviewName.c_str())) {
        ImGui::PushID("None");

        if (DrawAssetOption("None", nullptr, Current == nullptr)) {
            Setter({});
        }

        ImGui::PopID();

        TArray<FAssetHandle> MatchingAssets{Registry->GetAssetHandles(AssetType)};
        std::ranges::sort(MatchingAssets, [Registry](FAssetHandle Left, FAssetHandle Right) {
            return Registry->GetAssetPath(Left)->mPath < Registry->GetAssetPath(Right)->mPath;
        });

        for (FAssetHandle Handle : MatchingAssets) {
            const FString& AssetPath{Registry->GetAssetPath(Handle)->mPath};
            const std::size_t NameOffset{AssetPath.find_last_of('/') + 1};
            const char* AssetName{AssetPath.c_str() + NameOffset};
            ID3D11ShaderResourceView* Thumbnail{GetAssetThumbnail(Registry, Handle)};

            ImGui::PushID(Registry->ResolveAsset<UObject>(Handle));

            if (DrawAssetOption(AssetName, Thumbnail, Handle == CurrentHandle) && Handle != CurrentHandle) {
                Setter(Handle);
            }
            if (Handle == CurrentHandle) {
                ImGui::SetItemDefaultFocus();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", AssetPath.c_str());
            }

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();
}

void FPropertyEditorContext::UpdateTransformFields(const FTransform& Transform) {
    mEditPosition = Transform.GetPosition();
    mEditRotation = Transform.GetRotation();
    mEditScale = Transform.GetScale();
    mBAbsoluteLocation = Transform.IsAbsoluteLocation();
    mBAbsoluteRotation = Transform.IsAbsoluteRotation();
    mBAbsoluteScale = Transform.IsAbsoluteScale();
}

FTransform FPropertyEditorContext::BuildDesiredTransform() const {
    FTransform Transform{mEditPosition, mEditRotation, mEditScale};
    Transform.SetAbsoluteLocation(mBAbsoluteLocation);
    Transform.SetAbsoluteRotation(mBAbsoluteRotation);
    Transform.SetAbsoluteScale(mBAbsoluteScale);
    return Transform;
}

bool FPropertyEditorContext::SupportsAssetThumbnail(const FTypeInfo& AssetType) const {
    return UMesh::StaticTypeInfo()->IsA(&AssetType) || UMaterial::StaticTypeInfo()->IsA(&AssetType) || UTexture::StaticTypeInfo()->IsA(&AssetType);
}

ID3D11ShaderResourceView* FPropertyEditorContext::GetAssetThumbnail(const IAssetRegistry* Registry, FAssetHandle AssetHandle) const {
    const UAsset* Asset{Registry->ResolveAsset<UAsset>(AssetHandle)};

    if (Asset == nullptr) {
        return nullptr;
    }

    if (Asset->GetTypeInfo()->IsA(UTexture::StaticTypeInfo())) {
        return static_cast<const UTexture*>(Asset)->GetSRV();
    }

    if (mThumbnailRenderer == nullptr) {
        return nullptr;
    }

    if (Asset->GetTypeInfo()->IsA(UMesh::StaticTypeInfo()) || Asset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
        return mThumbnailRenderer->GetThumbnail(AssetHandle);
    }

    return nullptr;
}

bool FPropertyEditorContext::DrawAssetOption(const char* Label, ID3D11ShaderResourceView* Thumbnail, bool BSelected) const {
    constexpr float RowHeight{36.0f};
    constexpr float ImageSize{32.0f};
    constexpr float Padding{2.0f};

    const ImVec2 RowMin{ImGui::GetCursorScreenPos()};
    const ImVec2 RowSize{ImGui::GetContentRegionAvail().x, RowHeight};
    const bool BPressed{ImGui::Selectable("##AssetOption", BSelected, ImGuiSelectableFlags_None, RowSize)};

    ImDrawList* DrawList{ImGui::GetWindowDrawList()};
    const ImVec2 ImageMin{RowMin.x + Padding, RowMin.y + Padding};
    const ImVec2 ImageMax{ImageMin.x + ImageSize, ImageMin.y + ImageSize};

    if (Thumbnail != nullptr) {
        DrawList->AddImage(reinterpret_cast<ImTextureID>(Thumbnail), ImageMin, ImageMax);
    } else {
        DrawList->AddRect(ImageMin, ImageMax, ImGui::GetColorU32(ImGuiCol_Border));
    }

    const float TextY{RowMin.y + (RowHeight - ImGui::GetTextLineHeight()) * 0.5f};
    DrawList->AddText(ImVec2(ImageMax.x + 6.0f, TextY), ImGui::GetColorU32(ImGuiCol_Text), Label);

    return BPressed;
}

void FPropertyEditorContext::BindThumbnailRenderer(FAssetThumbnailRenderer* InThumbnailRenderer) {
    mThumbnailRenderer = InThumbnailRenderer;
}

void FPropertyEditorContext::BindAssetRegistry(const IAssetRegistry* InAssetRegistry) {
    mAssetRegistry = InAssetRegistry;
}
