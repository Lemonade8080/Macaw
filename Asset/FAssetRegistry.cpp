#include "pch.h"
#include "FAssetRegistry.h"

#include "UFreeTypeFont.h"
#include "UMesh.h"
#include "USurfaceOpaque.h"
#include "UTexture.h"
#include "FObjImporter.h"
#include "Asset/Pipeline/UPipeline.h"
#include "Core/Console/Console.h"
#include "Asset/Importer/FObjSerializer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace {
    constexpr const char* DefaultStaticMeshMaterialAssetPath{"/Game/System/Material/Default.mtl"};
    constexpr const char* DefaultStaticMeshPipelineAssetPath{"/Game/Pipeline/Base"};
    constexpr const char* DefaultCheckerboardTexturePath{"/Game/Texture/checkerboard.png"};
    constexpr const char* GizmoPipelineAssetPath{"/Game/Pipeline/Gizmo.json"};

    FString GetLowercaseExtension(const std::filesystem::path& FilePath) {
        FString Extension{FilePath.extension().generic_string().c_str()};
        std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
            return static_cast<char>(std::tolower(Character));
        });
        return Extension;
    }
}

bool FAssetRegistry::Initialize(ID3D11Device* Device, Uint32 MaxMaterialCount, const FProgressCallback& ProgressCallback) {
    if (Device == nullptr || !mMaterialBuffer.Initialize(Device, MaxMaterialCount)) {
        return false;
    }

    mDevice = Device;
    if (ProgressCallback) {
        ProgressCallback(0.0f, "Discovering assets");
    }

    if (!DiscoverAssets(std::filesystem::current_path() / "Content")) {
        return false;
    }

    const std::size_t TotalAssetCount{static_cast<std::size_t>(std::ranges::count_if(mAssets, [](const FAssetEntry& Entry) {
        return Entry.mAssetType != EAssetType::END && Entry.mAsset == nullptr;
    }))};
    std::size_t LoadedAssetCount{};
    const std::array AssetTypes{EAssetType::Texture, EAssetType::Font, EAssetType::Pipeline, EAssetType::Material, EAssetType::Mesh};
    bool LoadedAllAssets{true};

    for (const EAssetType AssetType : AssetTypes) {
        LoadedAllAssets = LoadAssetsOfType(Device, AssetType, LoadedAssetCount, TotalAssetCount, ProgressCallback) && LoadedAllAssets;
    }

    if (ProgressCallback) {
        ProgressCallback(1.0f, "Validating system assets");
    }

    return EnsureSystemAssets() && LoadedAllAssets;
}

bool FAssetRegistry::DiscoverAssets(const std::filesystem::path& Directory) {
    std::error_code ErrorCode{};
    const std::filesystem::path AbsoluteDirectory{std::filesystem::absolute(Directory, ErrorCode).lexically_normal()};

    if (ErrorCode || !std::filesystem::is_directory(AbsoluteDirectory, ErrorCode)) {
        return false;
    }

    mContentRoot = AbsoluteDirectory;

    std::filesystem::recursive_directory_iterator Iterator{mContentRoot, std::filesystem::directory_options::skip_permission_denied, ErrorCode};
    const std::filesystem::recursive_directory_iterator End{};

    bool BDiscoveredAll{true};

    while (!ErrorCode && Iterator != End) {
        const std::filesystem::directory_entry Entry{*Iterator};

        if (Entry.is_regular_file(ErrorCode)) {
            BDiscoveredAll = DiscoverAssetFile(Entry.path()) && BDiscoveredAll;
        }

        Iterator.increment(ErrorCode);

        if (ErrorCode) {
            ErrorCode.clear();
        }
    }

    return BDiscoveredAll;
}

bool FAssetRegistry::LoadAssetsOfType(ID3D11Device* Device, EAssetType AssetType) {
    std::size_t LoadedAssetCount{};
    return LoadAssetsOfType(Device, AssetType, LoadedAssetCount, 0, {});
}

bool FAssetRegistry::LoadAssetsOfType(ID3D11Device* Device, EAssetType AssetType, std::size_t& LoadedAssetCount, std::size_t TotalAssetCount, const FProgressCallback& ProgressCallback) {
    if (Device == nullptr) {
        return false;
    }

    bool BLoadedAll{true};

    for (FAssetEntry& Entry : mAssets) {
        if (Entry.mAssetType != AssetType || Entry.mAsset != nullptr) {
            continue;
        }

        std::string Status{"Loading "};
        Status += Entry.mAssetPath.mPath.c_str();
        if (ProgressCallback) {
            const float Progress{TotalAssetCount == 0 ? 0.0f : static_cast<float>(LoadedAssetCount) / static_cast<float>(TotalAssetCount)};
            ProgressCallback(Progress, Status);
        }

        bool BLoaded{false};

        if (AssetType == EAssetType::Texture) {
            BLoaded = LoadTexture(Entry, Device);
        } else if (AssetType == EAssetType::Font) {
            BLoaded = LoadFont(Entry, Device);
        } else if (AssetType == EAssetType::Pipeline) {
            BLoaded = LoadPipeline(Entry, Device);
        } else if (AssetType == EAssetType::Material) {
            BLoaded = LoadMaterial(Entry, Device);
        } else if (AssetType == EAssetType::Mesh) {
            BLoaded = LoadMesh(Entry, Device);
        }

        BLoadedAll = BLoadedAll && BLoaded;
        ++LoadedAssetCount;

        if (ProgressCallback) {
            const float Progress{TotalAssetCount == 0 ? 1.0f : static_cast<float>(LoadedAssetCount) / static_cast<float>(TotalAssetCount)};
            ProgressCallback(Progress, Status);
        }
    }

    return BLoadedAll;
}

FAssetHandle FAssetRegistry::FindAsset(const FAssetPath& AssetPath) const {
    const auto It{mPathToHandle.find(AssetPath)};

    if (It == mPathToHandle.end()) {
        return {};
    }

    return It->second;
}

FAssetHandle FAssetRegistry::FindAsset(const FGuid& PersistentGuid) const {
    const auto It{mGuidToHandle.find(PersistentGuid)};
    return It != mGuidToHandle.end() ? It->second : FAssetHandle{};
}

const FAssetPath* FAssetRegistry::GetAssetPath(FAssetHandle Handle) const {
    const FAssetEntry* Entry{FindEntry(Handle)};
    return Entry != nullptr && Entry->mAssetPath ? &Entry->mAssetPath : nullptr;
}

const FGuid* FAssetRegistry::GetAssetGuid(FAssetHandle Handle) const {
    const FAssetEntry* Entry{FindEntry(Handle)};
    return Entry != nullptr && Entry->mPersistentGuid.IsValid() ? &Entry->mPersistentGuid : nullptr;
}

bool FAssetRegistry::RemoveAsset(FAssetHandle Handle) {
    FAssetEntry* Entry{FindEntry(Handle)};

    if (Entry == nullptr) {
        return false;
    }

    if (Entry->mAsset != nullptr && Entry->mAsset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
        mMaterialBuffer.UnregisterMaterial(static_cast<UMaterial*>(Entry->mAsset.get()));
    }

    RemoveHandleMappings(Handle);
    Entry->mAsset.reset();
    Entry->mAssetPath = {};
    Entry->mPhysicalPath.clear();
    Entry->mSidecarPath.clear();
    Entry->mPersistentGuid = {};
    Entry->mAssetType = EAssetType::END;
    Entry->mHandle = FAssetHandle{Handle.mId, Handle.mGeneration + 1};
    mFreeHandles.push_back(Entry->mHandle);

    return true;
}

FAssetHandle FAssetRegistry::ImportMesh(const std::filesystem::path& SourceObjPath, const FString& TargetVirtualFolder) {
    std::error_code ErrorCode{};
    const std::filesystem::path AbsoluteSourcePath{std::filesystem::absolute(SourceObjPath, ErrorCode).lexically_normal()};
    if (ErrorCode || !std::filesystem::is_regular_file(AbsoluteSourcePath, ErrorCode) || GetLowercaseExtension(AbsoluteSourcePath) != ".obj") {
        return {};
    }

    const std::filesystem::path TargetFolder{ResolveContentFolder(TargetVirtualFolder)};
    if (TargetFolder.empty()) {
        return {};
    }

    std::filesystem::create_directories(TargetFolder, ErrorCode);
    if (ErrorCode) {
        return {};
    }

    std::filesystem::path TargetBinaryPath{TargetFolder / AbsoluteSourcePath.filename()};
    TargetBinaryPath.replace_extension(".bin");
    const bool BTargetExists{std::filesystem::exists(TargetBinaryPath, ErrorCode)};
    if (ErrorCode) {
        return {};
    }
    if (BTargetExists) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Mesh import target already exists: %s", TargetBinaryPath.generic_string().c_str());
        return {};
    }

    const std::filesystem::path TargetSidecarPath{MakeSidecarPath(TargetBinaryPath)};
    const bool SidecarExists{std::filesystem::exists(TargetSidecarPath, ErrorCode)};
    if (ErrorCode) {
        return {};
    }
    FAssetEntry ImportEntry{};
    if (!LoadOrCreateMetadata(TargetSidecarPath, EAssetType::Mesh, ImportEntry)) {
        return {};
    }

    FObjImporter Importer{};
    FGeometry Geometry{};
    if (!Importer.LoadObjFile(AbsoluteSourcePath.string().c_str(), Geometry) || !FObjSerializer::SaveBinary(Geometry, TargetBinaryPath.string().c_str())) {
        std::filesystem::remove(TargetBinaryPath, ErrorCode);
        if (!SidecarExists) {
            std::filesystem::remove(TargetSidecarPath, ErrorCode);
        }
        return {};
    }

    if (!DiscoverAssetFile(TargetBinaryPath)) {
        std::filesystem::remove(TargetBinaryPath, ErrorCode);
        if (!SidecarExists) {
            std::filesystem::remove(TargetSidecarPath, ErrorCode);
        }
        return {};
    }

    const FAssetHandle Handle{FindAsset(MakeAssetPath(TargetBinaryPath))};
    FAssetEntry* Entry{FindEntry(Handle)};
    if (Entry == nullptr || Entry->mAssetType != EAssetType::Mesh || !LoadMesh(*Entry, mDevice)) {
        if (Handle) {
            RemoveAsset(Handle);
        }
        std::filesystem::remove(TargetBinaryPath, ErrorCode);
        if (!SidecarExists) {
            std::filesystem::remove(TargetSidecarPath, ErrorCode);
        }
        return {};
    }

    return Handle;
}

FAssetHandle FAssetRegistry::LoadViewerAsset(const std::filesystem::path& SourcePath) {
    std::error_code ErrorCode{};
    const std::filesystem::path AbsolutePath{std::filesystem::absolute(SourcePath, ErrorCode).lexically_normal()};
    if (ErrorCode || mDevice == nullptr || !std::filesystem::is_regular_file(AbsolutePath, ErrorCode)) {
        return {};
    }

    for (const FAssetEntry& ExistingEntry : mAssets) {
        if (ExistingEntry.mAsset != nullptr && ExistingEntry.mPhysicalPath.lexically_normal() == AbsolutePath) {
            return ExistingEntry.mHandle;
        }
    }

    const FString Extension{GetLowercaseExtension(AbsolutePath)};
    const EAssetType AssetType{Extension == ".obj" ? EAssetType::Mesh : GetAssetType(AbsolutePath)};
    if (AssetType != EAssetType::Mesh && AssetType != EAssetType::Material && AssetType != EAssetType::Texture) {
        return {};
    }

    std::unique_ptr<UAsset> Asset{};
    if (AssetType == EAssetType::Texture) {
        std::unique_ptr<UTexture> Texture{std::make_unique<UTexture>()};
        if (!Texture->Initialize(mDevice, AbsolutePath, false, ETextureFormat::UNORM, true) || Texture->GetSRV() == nullptr) {
            return {};
        }
        Asset = std::move(Texture);
    } else if (AssetType == EAssetType::Material) {
        std::unique_ptr<USurfaceOpaque> Material{std::make_unique<USurfaceOpaque>()};
        if (!Material->Initialize(mDevice, AbsolutePath, [this](const std::filesystem::path& TexturePath) {
                return LoadViewerAsset(TexturePath);
            }) ||
            !mMaterialBuffer.RegisterMaterial(Material.get())) {
            return {};
        }
        Asset = std::move(Material);
    } else {
        std::unique_ptr<UMesh> Mesh{std::make_unique<UMesh>()};
        std::filesystem::path ObjPath{Extension == ".obj" ? AbsolutePath : std::filesystem::path{}};
        const std::filesystem::path BinPath{Extension == ".bin" ? AbsolutePath : std::filesystem::path{}};
        bool FlipUV{false};
        if (Extension == ".bin") {
            ObjPath = AbsolutePath;
            ObjPath.replace_extension(".obj");
            if (!std::filesystem::is_regular_file(ObjPath, ErrorCode)) {
                ObjPath.clear();
            }
            if (ErrorCode) {
                return {};
            }

            const std::filesystem::path SidecarPath{MakeSidecarPath(AbsolutePath)};
            if (std::filesystem::is_regular_file(SidecarPath, ErrorCode)) {
                FAssetEntry Metadata{};
                if (!LoadOrCreateMetadata(SidecarPath, EAssetType::Mesh, Metadata)) {
                    return {};
                }
                FlipUV = Metadata.mMeshMetadata.mFlipUV;
            }
            if (ErrorCode) {
                return {};
            }
        }
        if (!Mesh->Initialize(mDevice, ObjPath, BinPath, [this](const std::filesystem::path& MaterialPath) {
                return LoadViewerAsset(MaterialPath);
            },
                              [this](FAssetHandle MaterialHandle, const FString& GroupName) -> std::optional<Uint32> {
                                  const UMaterial* Material{ResolveAsset<UMaterial>(MaterialHandle)};
                                  return Material != nullptr ? Material->FindGroupIndex(GroupName) : std::nullopt;
                              },
                              FlipUV)) {
            return {};
        }
        Asset = std::move(Mesh);
    }

    const FAssetHandle Handle{AllocateHandle()};
    FAssetEntry Entry{};
    Entry.mAssetPath = FAssetPath{FString{"/Viewer/"} + std::to_string(Handle.mId).c_str() + "/" + AbsolutePath.filename().generic_string().c_str()};
    Entry.mPhysicalPath = AbsolutePath;
    Entry.mAssetType = AssetType;
    Entry.mHandle = Handle;
    Asset->SetAssetName(Entry.mAssetPath.mPath);
    Entry.mAsset = std::move(Asset);
    if (Handle.mId < mAssets.size()) {
        mAssets[Handle.mId] = std::move(Entry);
    } else {
        mAssets.emplace_back(std::move(Entry));
    }
    mPathToHandle[mAssets[Handle.mId].mAssetPath] = Handle;
    return Handle;
}

void FAssetRegistry::Reset() {
    mAssets.clear();
    mFreeHandles.clear();
    mPathToHandle.clear();
    mGuidToHandle.clear();
    mMaterialBuffer.Reset();
    mDevice = nullptr;
    mContentRoot.clear();
}

void FAssetRegistry::Finalize() {
    for (FAssetEntry& Entry : mAssets) {
        if (Entry.mAsset == nullptr || !Entry.mAsset->GetTypeInfo()->IsA(UMaterial::StaticTypeInfo())) {
            continue;
        }

        UMaterial* Material{static_cast<UMaterial*>(Entry.mAsset.get())};
        Material->Finalize(this);
        Material->MarkGPUDataDirty();
    }
}

FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshMaterial() const {
    const FAssetHandle DefaultMaterialHandle{FindAsset(FAssetPath{DefaultStaticMeshMaterialAssetPath})};
    if (ResolveAsset<UMaterial>(DefaultMaterialHandle) == nullptr) {
        return {};
    }

    return DefaultMaterialHandle;
}

FAssetHandle FAssetRegistry::EnsureDefaultStaticMeshPipeline() const {
    const FAssetHandle DefaultPipelineHandle{FindAsset(FAssetPath{DefaultStaticMeshPipelineAssetPath})};
    if (ResolveAsset<UPipeline>(DefaultPipelineHandle) == nullptr) {
        return {};
    }

    return DefaultPipelineHandle;
}

std::filesystem::path FAssetRegistry::ResolveContentFolder(const FString& VirtualFolder) const {
    constexpr std::string_view VirtualRoot{"/Game"};

    if (VirtualFolder == VirtualRoot) {
        return mContentRoot;
    }

    constexpr std::string_view VirtualPrefix{"/Game/"};

    if (!VirtualFolder.starts_with(VirtualPrefix)) {
        return {};
    }

    const FString RelativeFolder{VirtualFolder.substr(VirtualPrefix.size())};

    return (mContentRoot / std::filesystem::path(RelativeFolder)).lexically_normal();
}

bool FAssetRegistry::EnsureSystemAssets() {
    struct FSystemMeshDefinition {
        const char* mAssetPath{};
    };

    constexpr std::array SystemMeshes{ FSystemMeshDefinition{"/Game/System/Mesh/Capsule.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Cone.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Cube.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Cylinder.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/GizmoTorus.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Plane.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Pyramid.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/SkyDome.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Sphere.bin"}, FSystemMeshDefinition{"/Game/System/Mesh/Torus.bin"}};

    for (const FSystemMeshDefinition& Definition : SystemMeshes) {
        const FAssetHandle Handle{FindAsset(FAssetPath{Definition.mAssetPath})};
        if (ResolveAsset<UMesh>(Handle) == nullptr) {
            return false;
        }
    }

    struct FSystemAssetDefinition {
        const char* mAssetPath{};
    };

    constexpr std::array SystemGizmoMaterials{ FSystemAssetDefinition{"/Game/System/Material/Red.mtl"}, FSystemAssetDefinition{"/Game/System/Material/Green.mtl"}, FSystemAssetDefinition{"/Game/System/Material/Blue.mtl"}};

    for (const FSystemAssetDefinition& Definition : SystemGizmoMaterials) {
        const FAssetHandle Handle{FindAsset(FAssetPath{Definition.mAssetPath})};
        if (ResolveAsset<UMaterial>(Handle) == nullptr) {
            return false;
        }
    }

    const FAssetHandle GizmoPipelineHandle{FindAsset(FAssetPath{GizmoPipelineAssetPath})};
    if (ResolveAsset<UPipeline>(GizmoPipelineHandle) == nullptr) {
        return false;
    }
    return true;
}

bool FAssetRegistry::DiscoverAssetFile(const std::filesystem::path& FilePath) {
    const EAssetType AssetType{GetAssetType(FilePath)};

    if (AssetType == EAssetType::END) {
        return true;
    }

    const std::filesystem::path SidecarPath{MakeSidecarPath(FilePath)};
    FAssetEntry Entry{};
    if (!LoadOrCreateMetadata(SidecarPath, AssetType, Entry)) {
        return false;
    }

    if (AssetType == EAssetType::Pipeline && IsPipelineFamilyUnit(FilePath)) {
        const std::filesystem::path FamilyDirectory{FilePath.parent_path()};
        const std::filesystem::path FirstUnitPath{FindFirstPipelineFamilyUnit(FamilyDirectory)};
        if (FirstUnitPath.empty()) {
            return false;
        }

        if (FilePath.lexically_normal() != FirstUnitPath) {
            return true;
        }

        return RegisterDiscoveredAsset(MakeAssetPath(FamilyDirectory), FamilyDirectory, SidecarPath, Entry.mPersistentGuid, AssetType, Entry);
    }

    return RegisterDiscoveredAsset(MakeAssetPath(FilePath), FilePath, SidecarPath, Entry.mPersistentGuid, AssetType, Entry);
}

bool FAssetRegistry::LoadTexture(FAssetEntry& Entry, ID3D11Device* Device) {
    std::unique_ptr<UTexture> Texture{std::make_unique<UTexture>()};
    Texture->SetAssetName(Entry.mAssetPath.mPath);
    Texture->Initialize(Device, Entry.mPhysicalPath, Entry.mTextureMetadata.mMakeDDS, ETextureFormat::UNORM, Entry.mTextureMetadata.mGenerateMipMap);

    if (Texture->GetSRV() == nullptr) {
        return false;
    }

    Entry.mAsset = std::move(Texture);

    return true;
}

bool FAssetRegistry::LoadFont(FAssetEntry& Entry, ID3D11Device* Device) {
    std::unique_ptr<UFreeTypeFont> Font{std::make_unique<UFreeTypeFont>()};
    Font->SetAssetName(Entry.mAssetPath.mPath);

    if (!Font->Initialize(Device, Entry.mPhysicalPath) || Font->GetAtlasSRV() == nullptr) {
        return false;
    }

    Entry.mAsset = std::move(Font);

    return true;
}

bool FAssetRegistry::LoadPipeline(FAssetEntry& Entry, ID3D11Device* Device) {
    std::unique_ptr<UPipeline> Pipeline{std::make_unique<UPipeline>()};
    Pipeline->SetAssetName(Entry.mAssetPath.mPath);

    const bool BInitialized{Pipeline->Initialize(Device, Entry.mPhysicalPath)};

    if (!BInitialized) {
        return false;
    }

    Entry.mAsset = std::move(Pipeline);

    return true;
}

bool FAssetRegistry::LoadMaterial(FAssetEntry& Entry, ID3D11Device* Device) {
    std::unique_ptr<USurfaceOpaque> Material{std::make_unique<USurfaceOpaque>()};
    Material->SetAssetName(Entry.mAssetPath.mPath);

    const FAssetHandle CheckerboardHandle{FindAsset(FAssetPath{DefaultCheckerboardTexturePath})};
    if (ResolveAsset<UTexture>(CheckerboardHandle) == nullptr) {
        return false;
    }

    const bool BInitialized{Material->Initialize(Device, Entry.mPhysicalPath, [this, CheckerboardHandle](const std::filesystem::path& TexturePath) {
        const FAssetHandle TextureHandle{FindAsset(MakeAssetPath(TexturePath))};
        return ResolveAsset<UTexture>(TextureHandle) != nullptr ? TextureHandle : CheckerboardHandle;
    })};

    if (!BInitialized || !mMaterialBuffer.RegisterMaterial(Material.get())) {
        return false;
    }

    Entry.mAsset = std::move(Material);

    return true;
}

bool FAssetRegistry::LoadMesh(FAssetEntry& Entry, ID3D11Device* Device) {
    std::unique_ptr<UMesh> Mesh{std::make_unique<UMesh>()};
    Mesh->SetAssetName(Entry.mAssetPath.mPath);

    const bool BBinaryAsset{GetLowercaseExtension(Entry.mPhysicalPath) == ".bin"};

    std::filesystem::path SourceObjPath{Entry.mPhysicalPath};
    SourceObjPath.replace_extension(".obj");
    if (!std::filesystem::is_regular_file(SourceObjPath)) {
        SourceObjPath = std::filesystem::current_path() / "OBJFiles" / Entry.mPhysicalPath.filename();
        SourceObjPath.replace_extension(".obj");
    }

    std::filesystem::path BinaryPath{Entry.mPhysicalPath};
    if (!BBinaryAsset) {
        BinaryPath.replace_extension(".bin");
    }

    const bool BInitialized{Mesh->Initialize(
        Device,
        SourceObjPath,
        BinaryPath,
        [this](const std::filesystem::path& MaterialPath) {
            return FindAsset(MakeAssetPath(MaterialPath));
        },
        [this](FAssetHandle MaterialHandle, const FString& GroupName) -> std::optional<Uint32> {
            const UMaterial* Material{ResolveAsset<UMaterial>(MaterialHandle)};
            return Material != nullptr ? Material->FindGroupIndex(GroupName) : std::nullopt;
        },
        Entry.mMeshMetadata.mFlipUV)};

    if (!BInitialized) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to load model: %s", Entry.mPhysicalPath.generic_string().c_str());
        return false;
    }

    // Temporary LOD1 test: generate a 50% triangle-count mesh while loading.
    if (!Mesh->GenerateLOD1(Device, 0.01f)) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Failed to generate LOD1; using LOD0: %s", Entry.mPhysicalPath.generic_string().c_str());
    } else {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Generated LOD1: %s (%u -> %u triangles)", Entry.mPhysicalPath.generic_string().c_str(), Mesh->GetIndexCount(0) / 3, Mesh->GetIndexCount(1) / 3);
    }

    Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Loaded model: %s", Entry.mPhysicalPath.generic_string().c_str());
    Entry.mAsset = std::move(Mesh);

    return true;
}

bool FAssetRegistry::RegisterDiscoveredAsset(const FAssetPath& AssetPath, const std::filesystem::path& PhysicalPath, const std::filesystem::path& SidecarPath, const FGuid& PersistentGuid, EAssetType AssetType, FAssetEntry& Entry) {
    if (!AssetPath || !PersistentGuid.IsValid() || AssetType == EAssetType::END || mPathToHandle.contains(AssetPath) || mGuidToHandle.contains(PersistentGuid)) {
        return false;
    }

    const FAssetHandle Handle{AllocateHandle()};

    Entry.mAssetPath = AssetPath;
    Entry.mPhysicalPath = PhysicalPath;
    Entry.mSidecarPath = SidecarPath;
    Entry.mPersistentGuid = PersistentGuid;
    Entry.mAssetType = AssetType;
    Entry.mHandle = Handle;

    if (Handle.mId < mAssets.size()) {
        mAssets[Handle.mId] = std::move(Entry);
    } else {
        mAssets.emplace_back(std::move(Entry));
    }

    mPathToHandle[AssetPath] = Handle;
    mGuidToHandle[PersistentGuid] = Handle;
    return true;
}

std::filesystem::path FAssetRegistry::MakeSidecarPath(const std::filesystem::path& AssetPath) {
    return std::filesystem::path{AssetPath.string() + ".meta"};
}

bool FAssetRegistry::LoadOrCreateMetadata(const std::filesystem::path& SidecarPath, EAssetType AssetType, FAssetEntry& Entry) {
    if (std::filesystem::exists(SidecarPath)) {
        std::ifstream Input{SidecarPath};
        if (!Input.is_open()) {
            return false;
        }
        rapidjson::Document Document{};
        rapidjson::IStreamWrapper Stream{Input};
        Document.ParseStream(Stream);

        if (!Input.good() && !Input.eof()) {
            return false;
        }

        if (!Document.IsObject() || !Document.HasMember("Guid") || !Document["Guid"].IsString()) {
            return false;
        }

        if (!Entry.mPersistentGuid.Parse(Document["Guid"].GetString()) || !Entry.mPersistentGuid.IsValid()) {
            return false;
        }

        if (AssetType == EAssetType::Texture) {
            if (Document.HasMember("TextureFormat")) {
                if (!Document["TextureFormat"].IsBool()) {
                    return false;
                }
                Entry.mTextureMetadata.mMakeDDS = Document["TextureFormat"].GetBool();
            }
            if (Document.HasMember("GenerateMipMap")) {
                if (!Document["GenerateMipMap"].IsBool()) {
                    return false;
                }
                Entry.mTextureMetadata.mGenerateMipMap = Document["GenerateMipMap"].GetBool();
            }
        } else if (AssetType == EAssetType::Mesh && Document.HasMember("FlipUV")) {
            if (!Document["FlipUV"].IsBool()) {
                return false;
            }
            Entry.mMeshMetadata.mFlipUV = Document["FlipUV"].GetBool();
        }
        return true;
    }

    Entry.mPersistentGuid = FGuid::NewGuid();
    if (!Entry.mPersistentGuid.IsValid()) {
        return false;
    }

    rapidjson::Document Document{};
    Document.SetObject();
    rapidjson::Document::AllocatorType& Allocator{Document.GetAllocator()};
    const FString GuidString{Entry.mPersistentGuid.ToString()};
    Document.AddMember("Guid", rapidjson::Value(GuidString.c_str(), Allocator), Allocator);
    if (AssetType == EAssetType::Texture) {
        Document.AddMember("TextureFormat", Entry.mTextureMetadata.mMakeDDS, Allocator);
        Document.AddMember("GenerateMipMap", Entry.mTextureMetadata.mGenerateMipMap, Allocator);
    } else if (AssetType == EAssetType::Mesh) {
        Document.AddMember("FlipUV", Entry.mMeshMetadata.mFlipUV, Allocator);
    }

    rapidjson::StringBuffer Buffer{};
    rapidjson::PrettyWriter<rapidjson::StringBuffer> Writer{Buffer};
    Document.Accept(Writer);

    std::ofstream Output{SidecarPath, std::ios::binary | std::ios::trunc};
    if (!Output.is_open()) {
        return false;
    }

    Output << Buffer.GetString() << "\r\n";
    return Output.good();
}

bool FAssetRegistry::IsPipelineFamilyUnit(const std::filesystem::path& FilePath) {
    const std::filesystem::path FamilyDirectory{FilePath.parent_path()};
    return FilePath.extension() == ".json" && FamilyDirectory.parent_path().filename() == "Pipeline" && FilePath.stem().generic_string().starts_with(FamilyDirectory.filename().generic_string() + "_");
}

std::filesystem::path FAssetRegistry::FindFirstPipelineFamilyUnit(const std::filesystem::path& FamilyDirectory) {
    std::error_code ErrorCode{};
    std::vector<std::filesystem::path> UnitPaths{};
    for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(FamilyDirectory, ErrorCode)) {
        if (ErrorCode) {
            return {};
        }

        if (Entry.is_regular_file(ErrorCode) && IsPipelineFamilyUnit(Entry.path())) {
            UnitPaths.emplace_back(Entry.path().lexically_normal());
        }
    }

    if (ErrorCode || UnitPaths.empty()) {
        return {};
    }

    std::ranges::sort(UnitPaths, {}, [](const std::filesystem::path& Path) {
        return Path.filename().generic_string();
    });
    return UnitPaths.front();
}

FAssetPath FAssetRegistry::MakeAssetPath(const std::filesystem::path& PhysicalPath) const {
    std::error_code ErrorCode{};
    std::filesystem::path RelativePath{std::filesystem::relative(PhysicalPath, mContentRoot, ErrorCode).lexically_normal()};

    if (ErrorCode || RelativePath.empty() || *RelativePath.begin() == "..") {
        return {};
    }

    return FAssetPath{FString{"/Game/"} + RelativePath.generic_string().c_str()};
}

EAssetType FAssetRegistry::GetAssetType(const std::filesystem::path& FilePath) {
    const FString Extension{GetLowercaseExtension(FilePath)};

    if (Extension == ".bin") {
        return EAssetType::Mesh;
    }

    if (Extension == ".mtl") {
        return EAssetType::Material;
    }

    if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || Extension == ".dds" || Extension == ".tga" || Extension == ".bmp" || Extension == ".tif" || Extension == ".tiff" || Extension == ".gif" || Extension == ".hdr") {
        return EAssetType::Texture;
    }

    if (Extension == ".ttf" || Extension == ".otf") {
        return EAssetType::Font;
    }

    if (Extension == ".json" && std::ranges::any_of(FilePath.parent_path(), [](const std::filesystem::path& PathPart) {
            return PathPart == "Pipeline";
        })) {
        return EAssetType::Pipeline;
    }

    return EAssetType::END;
}

FAssetHandle FAssetRegistry::AllocateHandle() {
    if (!mFreeHandles.empty()) {
        const FAssetHandle Handle{mFreeHandles.back()};
        mFreeHandles.pop_back();
        return Handle;
    }

    return FAssetHandle{static_cast<Uint32>(mAssets.size()), 0};
}

FAssetEntry* FAssetRegistry::FindEntry(FAssetHandle Handle) {
    if (Handle.mId >= mAssets.size()) {
        return nullptr;
    }

    FAssetEntry& Entry{mAssets[Handle.mId]};
    return Entry.mHandle == Handle ? &Entry : nullptr;
}

const FAssetEntry* FAssetRegistry::FindEntry(FAssetHandle Handle) const {
    if (Handle.mId >= mAssets.size()) {
        return nullptr;
    }

    const FAssetEntry& Entry{mAssets[Handle.mId]};
    return Entry.mHandle == Handle ? &Entry : nullptr;
}

void FAssetRegistry::RemoveHandleMappings(FAssetHandle Handle) {
    for (auto It{mPathToHandle.begin()}; It != mPathToHandle.end();) {
        if (It->second == Handle) {
            It = mPathToHandle.erase(It);
        } else {
            ++It;
        }
    }

    for (auto It{mGuidToHandle.begin()}; It != mGuidToHandle.end();) {
        if (It->second == Handle) {
            It = mGuidToHandle.erase(It);
        } else {
            ++It;
        }
    }
}

const std::filesystem::path& FAssetRegistry::GetContentRoot() const {
    return mContentRoot;
}

const TArray<FAssetEntry>& FAssetRegistry::GetAssetEntries() const {
    return mAssets;
}

FMaterialBuffer& FAssetRegistry::GetMaterialBuffer() {
    return mMaterialBuffer;
}

const FMaterialBuffer& FAssetRegistry::GetMaterialBuffer() const {
    return mMaterialBuffer;
}

void FAssetRegistry::FlushMaterialBuffer(ID3D11DeviceContext* Context) {
    mMaterialBuffer.Flush(Context);
}

ID3D11ShaderResourceView* FAssetRegistry::GetMaterialBufferSRV() const {
    return *mMaterialBuffer.GetSRV();
}

void FAssetRegistry::FlushFontAtlas(FAssetHandle Handle, ID3D11DeviceContext* Context) {
    UFont* Font{ResolveAsset<UFont>(Handle)};
    if (Font != nullptr) {
        Font->FlushAtlas(Context);
    }
}

auto FAssetRegistry::GetAssetList() const {
    return mAssets | std::ranges::views::filter([](const FAssetEntry& Entry) {
               return Entry.mAsset != nullptr;
           }) |
           std::ranges::views::transform([](const FAssetEntry& Entry) -> UObject* {
               return Entry.mAsset.get();
           });
}

const UObject* FAssetRegistry::ResolveAssetObject(FAssetHandle Handle) const {
    const FAssetEntry* Entry{FindEntry(Handle)};
    return Entry != nullptr ? Entry->mAsset.get() : nullptr;
}

TArray<FAssetHandle> FAssetRegistry::GetAssetHandles(const FTypeInfo& AssetType) const {
    TArray<FAssetHandle> Handles{};
    for (const FAssetEntry& Entry : mAssets) {
        if (Entry.mAsset != nullptr && Entry.mAsset->GetTypeInfo()->IsA(&AssetType)) {
            Handles.push_back(Entry.mHandle);
        }
    }
    return Handles;
}

void FAssetRegistry::SetPipelineRenderMode(FAssetHandle Handle, ERenderMode Mode) {
    if (UPipeline* Pipeline{ResolveAsset<UPipeline>(Handle)}) {
        Pipeline->SetRenderMode(Mode);
    }
}

const FFontGlyph* FAssetRegistry::GetOrCreateFontGlyph(FAssetHandle Handle, char32_t CodePoint) {
    UFont* Font{ResolveAsset<UFont>(Handle)};
    return Font != nullptr ? Font->GetOrCreateGlyph(CodePoint) : nullptr;
}
