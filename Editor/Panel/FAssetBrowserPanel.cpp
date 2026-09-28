#include "pch.h"

#include "Editor/Panel/FAssetBrowserPanel.h"
#include "Editor/FileDialog.h"

#include "Asset/FAssetRegistry.h"
#include "Asset/UTexture.h"
#include "Asset/UMesh.h"
#include "World/FWorldEditorContext.h"
#include "Core/Console/Console.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string_view>

namespace {
    constexpr float FolderPaneWidth{190.0f};
    constexpr float ThumbnailSize{96.0f};
    constexpr float TileWidth{ThumbnailSize + 18.0f};
    constexpr char StaticMeshAssetPayloadType[]{"MACAW_STATIC_MESH_ASSET"};
    constexpr char MaterialAssetPayloadType[]{"MACAW_MATERIAL_ASSET"};
    constexpr char TextureAssetPayloadType[]{"MACAW_TEXTURE_ASSET"};
}

FAssetBrowserPanel::FAssetBrowserPanel(FAssetRegistry& InAssetRegistry, FWorldEditorContext& InEditorContext, FAssetThumbnailRenderer* InThumbnailRenderer, std::function<void(FAssetHandle)> InOpenMaterialEditor)
	: FEditorWindow{"Content Browser###AssetBrowserPanel"},
	  mAssetRegistry{&InAssetRegistry},
	  mEditorContext{InEditorContext},
	  mThumbnailRenderer{InThumbnailRenderer},
	  mOpenMaterialEditor{std::move(InOpenMaterialEditor)} {
}

void FAssetBrowserPanel::BeginExternalDropFrame() {
    mBDropTargetActive = false;
}

bool FAssetBrowserPanel::HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition) {
    if (!mBDropTargetActive || mAssetRegistry == nullptr ||
        ScreenPosition.x < mDropTargetMin.x || ScreenPosition.x >= mDropTargetMax.x ||
        ScreenPosition.y < mDropTargetMin.y || ScreenPosition.y >= mDropTargetMax.y) {
        return false;
    }

    FString Extension{FilePath.extension().generic_string().c_str()};
    std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });

    if (Extension != ".obj") {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Only OBJ files can be dropped into the Asset Browser: %s", FilePath.generic_string().c_str());
        return true;
    }

    const FAssetHandle ImportedHandle{mAssetRegistry->ImportMesh(FilePath, mSelectedFolder)};
    if (ImportedHandle) {
        if (mThumbnailRenderer != nullptr) {
            mThumbnailRenderer->RenderThumbnail(ImportedHandle);
        }
        mSelectedAsset = ImportedHandle;
        Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Imported dropped OBJ into %s: %s", mSelectedFolder.c_str(), FilePath.generic_string().c_str());
    } else {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to import dropped OBJ into %s: %s", mSelectedFolder.c_str(), FilePath.generic_string().c_str());
    }

    return true;
}

void FAssetBrowserPanel::DrawContents() {
    const ImVec2 WindowPosition{ImGui::GetWindowPos()};
    const ImVec2 WindowSize{ImGui::GetWindowSize()};
    mDropTargetMin = WindowPosition;
    mDropTargetMax = ImVec2(WindowPosition.x + WindowSize.x, WindowPosition.y + WindowSize.y);
    mBDropTargetActive = true;

    ImGui::TextDisabled("Content");
    ImGui::SameLine();
    ImGui::TextUnformatted(mSelectedFolder.c_str());
    ImGui::SameLine();

    if (ImGui::Button("Import")) {
        const FString FilePath{OpenFileDialog(nullptr, "./Content", "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0", "obj")};

        if (!FilePath.empty()) {
            const std::filesystem::path SourcePath{FilePath};
            const FAssetHandle ImportedHandle{mAssetRegistry->ImportMesh(SourcePath, mSelectedFolder)};
            if (ImportedHandle && mThumbnailRenderer != nullptr) {
                mThumbnailRenderer->RenderThumbnail(ImportedHandle);
            }
        }
    }

    ImGui::TextUnformatted("Type");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::BeginCombo("##AssetTypeFilter", mSelectedAssetType == EAssetType::END ? "All Types" : GetAssetTypeLabel(mSelectedAssetType))) {
        if (ImGui::Selectable("All Types", mSelectedAssetType == EAssetType::END)) {
            mSelectedAssetType = EAssetType::END;
        }
        if (ImGui::Selectable("Static Mesh", mSelectedAssetType == EAssetType::Mesh)) {
            mSelectedAssetType = EAssetType::Mesh;
        }
        if (ImGui::Selectable("Texture", mSelectedAssetType == EAssetType::Texture)) {
            mSelectedAssetType = EAssetType::Texture;
        }
        if (ImGui::Selectable("Material", mSelectedAssetType == EAssetType::Material)) {
            mSelectedAssetType = EAssetType::Material;
        }
        if (ImGui::Selectable("Pipeline", mSelectedAssetType == EAssetType::Pipeline)) {
            mSelectedAssetType = EAssetType::Pipeline;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##AssetFilter", "Search assets", mAssetFilter.InputBuf, IM_ARRAYSIZE(mAssetFilter.InputBuf))) {
        mAssetFilter.Build();
    }
    ImGui::Separator();

    const float ContentHeight{ImGui::GetContentRegionAvail().y};
    if (ImGui::BeginChild("AssetFolders", ImVec2(FolderPaneWidth, ContentHeight), ImGuiChildFlags_Borders)) {
        DrawFolderTree("/Game", "Content");
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("AssetTiles", ImVec2(0.0f, ContentHeight), ImGuiChildFlags_Borders)) {
        std::vector<const FAssetEntry*> VisibleAssets{};
        for (const FAssetEntry& Entry : mAssetRegistry->GetAssetEntries()) {
            if (!IsInSelectedFolder(Entry) || (mSelectedAssetType != EAssetType::END && Entry.mAssetType != mSelectedAssetType)) {
                continue;
            }

            const std::string_view AssetPath{Entry.mAssetPath.mPath.data(), Entry.mAssetPath.mPath.size()};
            if (!mAssetFilter.PassFilter(AssetPath.data(), AssetPath.data() + AssetPath.size())) {
                continue;
            }

            VisibleAssets.push_back(&Entry);
        }

        std::ranges::sort(VisibleAssets, [](const FAssetEntry* Left, const FAssetEntry* Right) {
            return Left->mAssetPath.mPath < Right->mAssetPath.mPath;
        });

        const int ColumnCount{std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / TileWidth))};
        if (ImGui::BeginTable("AssetGrid", ColumnCount, ImGuiTableFlags_SizingFixedFit)) {
            for (int AssetIndex{0}; AssetIndex < static_cast<int>(VisibleAssets.size()); ++AssetIndex) {
                ImGui::TableNextColumn();
                DrawAssetTile(*VisibleAssets[AssetIndex]);
            }
            ImGui::EndTable();
        }

        if (VisibleAssets.empty()) {
            ImGui::TextDisabled("No assets in this folder.");
        }
    }

    ImGui::EndChild();
}

void FAssetBrowserPanel::PushWindowStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.080f, 0.095f, 1.0f));
}

void FAssetBrowserPanel::PopWindowStyle() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

FString FAssetBrowserPanel::GetParentFolder(const FString& AssetPath) {
    const std::size_t SlashIndex{AssetPath.find_last_of('/')};
    return SlashIndex == FString::npos ? FString{} : AssetPath.substr(0, SlashIndex);
}

const char* FAssetBrowserPanel::GetAssetTypeLabel(EAssetType AssetType) {
    switch (AssetType) {
        case EAssetType::Texture:
            return "Texture";
        case EAssetType::Pipeline:
            return "Pipeline";
        case EAssetType::Material:
            return "Material";
        case EAssetType::Mesh:
            return "Static Mesh";
        case EAssetType::Font:
            return "Font";
        default:
            return "Asset";
    }
}

bool FAssetBrowserPanel::IsInSelectedFolder(const FAssetEntry& Entry) const {
    if (mSelectedFolder == "/Game") {
        return true;
    }

    return GetParentFolder(Entry.mAssetPath.mPath) == mSelectedFolder;
}

void FAssetBrowserPanel::DrawFolderTree(const FString& FolderPath, const char* FolderName) {
    std::vector<FString> ChildFolderNames{};
    const std::string_view FolderPathView{FolderPath.data(), FolderPath.size()};

    for (const FAssetEntry& Entry : mAssetRegistry->GetAssetEntries()) {
        const std::string_view AssetPath{Entry.mAssetPath.mPath.data(), Entry.mAssetPath.mPath.size()};
        if (!AssetPath.starts_with(FolderPathView) || AssetPath.size() <= FolderPathView.size() || AssetPath[FolderPathView.size()] != '/') {
            continue;
        }

        const std::string_view RemainingPath{AssetPath.substr(FolderPathView.size() + 1)};
        const std::size_t SeparatorIndex{RemainingPath.find('/')};
        if (SeparatorIndex == std::string_view::npos) {
            continue;
        }

        const FString ChildName{RemainingPath.substr(0, SeparatorIndex).data(), SeparatorIndex};
        if (std::ranges::find(ChildFolderNames, ChildName) == ChildFolderNames.end()) {
            ChildFolderNames.push_back(ChildName);
        }
    }

    std::ranges::sort(ChildFolderNames);
    const bool BHasChildren{!ChildFolderNames.empty()};
    ImGuiTreeNodeFlags Flags{ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow};
    if (!BHasChildren) {
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (mSelectedFolder == FolderPath) {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool BOpen{ImGui::TreeNodeEx(FolderPath.c_str(), Flags, "%s", FolderName)};
    if (ImGui::IsItemClicked()) {
        mSelectedFolder = FolderPath;
    }

    if (BHasChildren && BOpen) {
        for (const FString& ChildName : ChildFolderNames) {
            DrawFolderTree(FolderPath + "/" + ChildName, ChildName.c_str());
        }
        ImGui::TreePop();
    }
}

void FAssetBrowserPanel::DrawAssetTile(const FAssetEntry& Entry) {
    const FString& AssetPath{Entry.mAssetPath.mPath};
    const std::size_t NameOffset{AssetPath.find_last_of('/') + 1};
    const char* AssetName{AssetPath.c_str() + NameOffset};
    const bool BSelected{mSelectedAsset == Entry.mHandle};

    ImGui::PushID(static_cast<int>(Entry.mHandle.mId));
    if (BSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.36f, 0.66f, 1.0f));
    }

    ID3D11ShaderResourceView* ThumbnailSRV{nullptr};

    if (Entry.mAssetType == EAssetType::Texture) {
        const UTexture* Texture{mAssetRegistry->ResolveAsset<UTexture>(Entry.mHandle)};

        if (Texture != nullptr) {
            ThumbnailSRV = Texture->GetSRV();
        }
    } else if (Entry.mAssetType == EAssetType::Mesh || Entry.mAssetType == EAssetType::Material) {
        ThumbnailSRV = mThumbnailRenderer->GetThumbnail(Entry.mHandle);
    }

    bool BClicked{false};

    if (ThumbnailSRV != nullptr) {
        BClicked = ImGui::ImageButton("##Thumbnail", ImTextureRef(reinterpret_cast<ImTextureID>(ThumbnailSRV)), ImVec2(ThumbnailSize, ThumbnailSize));
    } else {
        BClicked = ImGui::Button(GetAssetTypeLabel(Entry.mAssetType), ImVec2(ThumbnailSize, ThumbnailSize));
    }

    if (BSelected) {
        ImGui::PopStyleColor();
    }
    if (BClicked) {
        mSelectedAsset = Entry.mHandle;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\n%s", AssetPath.c_str(), GetAssetTypeLabel(Entry.mAssetType));

        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (Entry.mAssetType == EAssetType::Mesh) {
                mEditorContext.SetPreviewMesh(Entry.mHandle);
            } else if (Entry.mAssetType == EAssetType::Material && mOpenMaterialEditor) {
                mOpenMaterialEditor(Entry.mHandle);
            }
        }
    }

    if (Entry.mAssetType == EAssetType::Mesh && ImGui::BeginDragDropSource()) {
        const FAssetHandle MeshHandle{Entry.mHandle};
        ImGui::SetDragDropPayload(StaticMeshAssetPayloadType, &MeshHandle, sizeof(MeshHandle));
        ImGui::TextUnformatted(AssetName);
        ImGui::TextDisabled("Static Mesh");
        ImGui::EndDragDropSource();
    } else if (Entry.mAssetType == EAssetType::Material && ImGui::BeginDragDropSource()) {
        const FAssetHandle MaterialHandle{Entry.mHandle};
        ImGui::SetDragDropPayload(MaterialAssetPayloadType, &MaterialHandle, sizeof(MaterialHandle));
        ImGui::TextUnformatted(AssetName);
        ImGui::TextDisabled("Material");
        ImGui::EndDragDropSource();
    } else if (Entry.mAssetType == EAssetType::Texture && ImGui::BeginDragDropSource()) {
        const FAssetHandle TextureHandle{Entry.mHandle};
        ImGui::SetDragDropPayload(TextureAssetPayloadType, &TextureHandle, sizeof(TextureHandle));
        ImGui::TextUnformatted(AssetName);
        ImGui::TextDisabled("Texture");
        ImGui::EndDragDropSource();
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ThumbnailSize);
    ImGui::TextUnformatted(AssetName);
    ImGui::PopTextWrapPos();
    ImGui::TextDisabled("%s", GetAssetTypeLabel(Entry.mAssetType));
    ImGui::PopID();
}
