#pragma once

#include <filesystem>

class FAssetRegistry;
class UWorld;

class FTemporarySceneLoader {
public:
    bool Load(const std::filesystem::path& ScenePath, UWorld& World, FAssetRegistry& AssetRegistry);
};
