#include "pch.h"
#include "FEditorConfigManager.h"
#include "Serialization/FArchiveJson.h"

#include <fstream>
#include <sstream>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/ostreamwrapper.h"

bool FEditorConfigManager::Save(FEditorSettings& Settings, const IAssetRegistry* AssetRegistry, const std::filesystem::path& ConfigPath) {
    std::filesystem::path CurrentPath{std::filesystem::current_path()};
    std::filesystem::path FilePath{CurrentPath / ConfigPath};

    rapidjson::Document Document{};
    Document.SetObject();
    rapidjson::Document::AllocatorType& Allocator{Document.GetAllocator()};

    FArchiveJson ArchiveSave{Document, Allocator};
    if (AssetRegistry) {
        ArchiveSave.SetAssetRegistry(AssetRegistry);
    }

    ArchiveSave.SerializeStruct("EditorSettings", Settings);

    std::ofstream OutputFileStream{FilePath};
    if (!OutputFileStream.is_open()) {
        return false;
    }

    rapidjson::OStreamWrapper StreamWrapper{OutputFileStream};
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> Writer{StreamWrapper};
    Document.Accept(Writer);
    OutputFileStream.close();

    return true;
}

bool FEditorConfigManager::Load(FEditorSettings& OutSettings, const IAssetRegistry* AssetRegistry, const std::filesystem::path& ConfigPath) {
    std::filesystem::path CurrentPath{std::filesystem::current_path()};
    std::filesystem::path FilePath{CurrentPath / ConfigPath};

    std::ifstream InputFileStream{FilePath};
    if (!InputFileStream) {
        return false;
    }

    std::stringstream Buffer{};
    Buffer << InputFileStream.rdbuf();
    std::string LoadedJsonString{Buffer.str()};
    InputFileStream.close();

    rapidjson::Document LoadDocument{};
    LoadDocument.Parse(LoadedJsonString.c_str());

    if (LoadDocument.HasParseError() || !LoadDocument.IsObject() || !LoadDocument.HasMember("EditorSettings") || !LoadDocument["EditorSettings"].IsObject()) {
        return false;
    }

    rapidjson::Value& SettingsJson{LoadDocument["EditorSettings"]};
    FArchiveJson ArchiveLoad{SettingsJson};
    if (AssetRegistry) {
        ArchiveLoad.SetAssetRegistry(AssetRegistry);
    }

    OutSettings.Serialize(ArchiveLoad);

    return true;
}
