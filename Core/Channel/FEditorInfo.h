#pragma once

#include "Core/Common.h"
#include "Core/Base/TypeInfo.h"

#define JG_DECLARE_EDITOR_MESSAGE(MessageType)                              \
    inline static const FTypeInfo TypeInfo{#MessageType, nullptr, nullptr}; \
    static const FTypeInfo& StaticTypeInfo() noexcept {                     \
        return TypeInfo;                                                    \
    }                                                                       \
    MessageType() = default;                                                \
    ~MessageType() = default;                                               \
    MessageType(const MessageType&) = default;                              \
    MessageType& operator=(const MessageType&) = default;                   \
    MessageType(MessageType&&) noexcept = default;                          \
    MessageType& operator=(MessageType&&) noexcept = default

// =========================================================
// [Event] 단방향 메시지 데이터 (FMessageChannel 용)
// =========================================================
struct FMessageSpawnComponent {
    FString mComponentType{};
    FString mMeshType{};
    Uint32 mSpawnCount{};

    JG_DECLARE_EDITOR_MESSAGE(FMessageSpawnComponent);

    FMessageSpawnComponent(FString InputComponentType, FString InputMeshType, Uint32 InputCount) noexcept;
};

struct FMessageSaveScene {
    FString mSceneName{};

    JG_DECLARE_EDITOR_MESSAGE(FMessageSaveScene);

    FMessageSaveScene(FString InputSceneName) noexcept;
};

struct FMessageLoadScene {
    FString mFilePath{};

    JG_DECLARE_EDITOR_MESSAGE(FMessageLoadScene);

    FMessageLoadScene(FString InputFilePath) noexcept;
};

struct FMessageImportMesh {
    FString mAssetName{};
    FString mFilePath{};
    FString mMetaPath{};

    JG_DECLARE_EDITOR_MESSAGE(FMessageImportMesh);

    FMessageImportMesh(FString InputAssetName, FString InputFilePath, FString InputMetaPath) noexcept;
};

enum class EGizmoMode : Uint8 {
    Translate,
    Rotate,
    Scale
};

enum class EGizmoCoordinateSpace : Uint8 {
    World,
    Local
};

enum class EStatDisplayMode : Uint8 {
    Fps,
    Memory,
    None
};

struct FStatDisplayFlags {
    bool mBShowFps{false};
    bool mBShowMemory{false};
    bool mBObjectSystem{false};
    bool mBShowPicking{false};
    bool mBShowRender{false};
    bool mBShowLOD{false};
};
