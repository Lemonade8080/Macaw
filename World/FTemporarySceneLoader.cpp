#include "pch.h"
#include "FTemporarySceneLoader.h"

#include "AActor.h"
#include "UWorld.h"
#include "Asset/FAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"
#include "Asset/Pipeline/UPipeline.h"
#include "Component/UCameraComponent.h"
#include "Component/UStaticMeshComponent.h"
#include "Core/Asset/FAssetPath.h"
#include "Core/Base/FTransform.h"

#include <cmath>
#include <fstream>
#include <numbers>
#include <string>
#include <unordered_map>
#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

namespace {
    struct FTemporaryPrimitive {
        std::string mMeshPath{};
        FVector3 mLocation{};
        FRotator mRotation{};
        FVector3 mScale{1.0f, 1.0f, 1.0f};
    };

    struct FTemporaryCamera {
        FVector3 mLocation{};
        FRotator mRotation{};
        float mFieldOfView{};
        float mNearPlane{};
        float mFarPlane{};
    };

    struct FTemporaryMeshAssets {
        FAssetHandle mMesh{};
        FAssetHandle mMaterial{};
    };

    bool ReadVector3(const rapidjson::Value& Value, FVector3& Result) {
        if (!Value.IsArray() || Value.Size() != 3 || !Value[0].IsNumber() || !Value[1].IsNumber() || !Value[2].IsNumber()) {
            return false;
        }

        const FVector3 Parsed{Value[0].GetFloat(), Value[1].GetFloat(), Value[2].GetFloat()};
        if (!std::isfinite(Parsed.mX) || !std::isfinite(Parsed.mY) || !std::isfinite(Parsed.mZ)) {
            return false;
        }

        Result = Parsed;
        return true;
    }

    bool ReadSingleNumber(const rapidjson::Value& Value, float& Result) {
        if (!Value.IsArray() || Value.Size() != 1 || !Value[0].IsNumber()) {
            return false;
        }

        const float Parsed{Value[0].GetFloat()};
        if (!std::isfinite(Parsed)) {
            return false;
        }

        Result = Parsed;
        return true;
    }

    bool ReadCamera(const rapidjson::Value& Value, FTemporaryCamera& Result) {
        if (!Value.IsObject() || !Value.HasMember("Location") || !Value.HasMember("Rotation") || !Value.HasMember("FOV") || !Value.HasMember("NearClip") || !Value.HasMember("FarClip")) {
            return false;
        }

        FVector3 Rotation{};
        if (!ReadVector3(Value["Location"], Result.mLocation) || !ReadVector3(Value["Rotation"], Rotation) || !ReadSingleNumber(Value["FOV"], Result.mFieldOfView) || !ReadSingleNumber(Value["NearClip"], Result.mNearPlane) || !ReadSingleNumber(Value["FarClip"], Result.mFarPlane)) {
            return false;
        }

        if (Result.mFieldOfView <= 0.0f || Result.mFieldOfView >= 180.0f || Result.mNearPlane <= 0.0f || Result.mFarPlane <= Result.mNearPlane) {
            return false;
        }

        Result.mRotation = FRotator{Rotation};
        return true;
    }

    bool ReadPrimitive(const rapidjson::Value& Value, FTemporaryPrimitive& Result) {
        if (!Value.IsObject() || !Value.HasMember("Type") || !Value["Type"].IsString() || std::string_view{Value["Type"].GetString()} != "StaticMeshComp" || !Value.HasMember("Location") || !Value.HasMember("Rotation") || !Value.HasMember("Scale") || !Value.HasMember("ObjStaticMeshAsset") || !Value["ObjStaticMeshAsset"].IsString()) {
            return false;
        }

        FVector3 Rotation{};
        if (!ReadVector3(Value["Location"], Result.mLocation) || !ReadVector3(Value["Rotation"], Rotation) || !ReadVector3(Value["Scale"], Result.mScale)) {
            return false;
        }

        const std::filesystem::path MeshPath{Value["ObjStaticMeshAsset"].GetString()};
        if (MeshPath.is_absolute() || MeshPath.has_root_path() || MeshPath.extension() != ".obj") {
            return false;
        }

        for (const std::filesystem::path& Part : MeshPath) {
            if (Part == "..") {
                return false;
            }
        }

        if (!MeshPath.generic_string().starts_with("Data/")) {
            return false;
        }

        Result.mMeshPath = MeshPath.lexically_normal().generic_string();
        Result.mRotation = FRotator{Rotation};
        return true;
    }

    bool LoadMeshAssets(const std::string& RelativeMeshPath, FAssetRegistry& AssetRegistry, FTemporaryMeshAssets& Result) {
        const std::filesystem::path RelativePath{RelativeMeshPath};
        std::filesystem::path RelativeMaterialPath{RelativePath};
        RelativeMaterialPath.replace_extension(".mtl");
        std::filesystem::path RelativeBinaryPath{RelativePath};
        RelativeBinaryPath.replace_extension(".bin");

        const std::string MaterialAssetPath{std::string{"/Game/"} + RelativeMaterialPath.generic_string()};
        const std::string MeshAssetPath{std::string{"/Game/"} + RelativeBinaryPath.generic_string()};
        Result.mMaterial = AssetRegistry.FindAsset(FAssetPath{MaterialAssetPath.c_str()});
        if (AssetRegistry.ResolveAsset<UMaterial>(Result.mMaterial) == nullptr) {
            return false;
        }

        Result.mMesh = AssetRegistry.FindAsset(FAssetPath{MeshAssetPath.c_str()});
        if (AssetRegistry.ResolveAsset<UMesh>(Result.mMesh) == nullptr) {
            const std::filesystem::path BinaryPath{AssetRegistry.GetContentRoot() / RelativeBinaryPath};
            std::error_code ErrorCode{};
            const bool BHasBinary{std::filesystem::exists(BinaryPath, ErrorCode)};
            if (ErrorCode) {
                return false;
            }
            if (!BHasBinary) {
                const std::filesystem::path MeshPath{AssetRegistry.GetContentRoot() / RelativePath};
                const std::string TargetVirtualFolder{std::string{"/Game/"} + RelativePath.parent_path().generic_string()};
                Result.mMesh = AssetRegistry.ImportMesh(MeshPath, FString{TargetVirtualFolder.c_str()});
            }
        }
        return AssetRegistry.ResolveAsset<UMesh>(Result.mMesh) != nullptr;
    }
}

bool FTemporarySceneLoader::Load(const std::filesystem::path& ScenePath, UWorld& World, FAssetRegistry& AssetRegistry) {
    std::ifstream Input{ScenePath, std::ios::binary};
    if (!Input.is_open()) {
        return false;
    }

    rapidjson::IStreamWrapper Stream{Input};
    rapidjson::Document Document{};
    Document.ParseStream(Stream);
    if (Document.HasParseError() || !Document.IsObject() || !Document.HasMember("PerspectiveCamera") || !Document.HasMember("Primitives") || !Document["Primitives"].IsObject()) {
        return false;
    }

    FTemporaryCamera Camera{};
    if (!ReadCamera(Document["PerspectiveCamera"], Camera)) {
        return false;
    }

    const rapidjson::Value& JsonPrimitives{Document["Primitives"]};
    std::vector<FTemporaryPrimitive> Primitives{};
    Primitives.reserve(JsonPrimitives.MemberCount());
    for (auto Iterator{JsonPrimitives.MemberBegin()}; Iterator != JsonPrimitives.MemberEnd(); ++Iterator) {
        FTemporaryPrimitive Primitive{};
        if (!ReadPrimitive(Iterator->value, Primitive)) {
            return false;
        }
        Primitives.push_back(std::move(Primitive));
    }

    std::unordered_map<std::string, FTemporaryMeshAssets> MeshAssets{};
    for (const FTemporaryPrimitive& Primitive : Primitives) {
        if (MeshAssets.contains(Primitive.mMeshPath)) {
            continue;
        }

        FTemporaryMeshAssets Assets{};
        if (!LoadMeshAssets(Primitive.mMeshPath, AssetRegistry, Assets)) {
            return false;
        }
        MeshAssets.emplace(Primitive.mMeshPath, Assets);
    }

    const FAssetHandle Pipeline{AssetRegistry.FindAsset(FAssetPath{"/Game/Pipeline/TexturedBase"})};
    if (AssetRegistry.ResolveAsset<UPipeline>(Pipeline) == nullptr) {
        return false;
    }

    World.ClearActors();

    std::unique_ptr<AActor> CameraOwner{std::make_unique<AActor>()};
    CameraOwner->SetName(FName{"PerspectiveCamera"});
    AActor* CameraActor{World.AddActor(std::move(CameraOwner))};
    UCameraComponent* CameraComponent{CameraActor != nullptr ? CameraActor->AddComponent<UCameraComponent>() : nullptr};
    if (CameraComponent == nullptr || !CameraActor->SetRootComponent(CameraComponent)) {
        World.ClearActors();
        return false;
    }
    CameraComponent->SetRelativeTransform(FTransform{Camera.mLocation, Camera.mRotation, FVector3{1.0f, 1.0f, 1.0f}});
    CameraComponent->SetFOV(Camera.mFieldOfView * std::numbers::pi_v<float> / 180.0f);
    CameraComponent->SetNearPlane(Camera.mNearPlane);
    CameraComponent->SetFarPlane(Camera.mFarPlane);

    Int32 PrimitiveNumber{1};
    for (const FTemporaryPrimitive& Primitive : Primitives) {
        const FTemporaryMeshAssets& Assets{MeshAssets.at(Primitive.mMeshPath)};
        std::unique_ptr<AActor> Owner{std::make_unique<AActor>()};
        Owner->SetName(FName{"Primitive", PrimitiveNumber});
        AActor* Actor{World.AddActor(std::move(Owner))};
        UStaticMeshComponent* MeshComponent{Actor != nullptr ? Actor->AddComponent<UStaticMeshComponent>() : nullptr};
        if (MeshComponent == nullptr || !Actor->SetRootComponent(MeshComponent)) {
            World.ClearActors();
            return false;
        }

        MeshComponent->SetMeshHandle(Assets.mMesh);
        MeshComponent->SetMaterialHandle(Assets.mMaterial);
        MeshComponent->SetPipelineHandle(Pipeline);
        MeshComponent->SetRelativeTransform(FTransform{Primitive.mLocation, Primitive.mRotation, Primitive.mScale});
        ++PrimitiveNumber;
    }

    return true;
}
