#pragma once

#include "pch.h"

enum class ELogLevel {
    Log,
    Warning,
    Error,
    Fatal
};
enum class ELogCategory {
    Core,
    Render,
    Physics,
    Etc
};

struct FConsoleMessage {
    FString mTime{};
    ELogLevel mLevel{ELogLevel::Log};
    ELogCategory mCategory{ELogCategory::Etc};
    FString mText{};
};

struct FConsoleOutputHandle {
    std::uint32_t mIndex{UINT32_MAX};

    bool IsValid() const;
};

namespace Console {
    inline constexpr FConsoleOutputHandle STDOutHandle{0};
    inline constexpr FConsoleOutputHandle STDErrorHandle{1};

    void Print(FConsoleOutputHandle Handle, FConsoleMessage Message);
    void Clear(FConsoleOutputHandle Handle);
    void AddLog(FConsoleOutputHandle Handle, ELogLevel Level, ELogCategory Categor, const char* Format, ...);
    void Flush(FConsoleOutputHandle Handle);

    std::size_t GetMessageCount(FConsoleOutputHandle Handle);
    const FConsoleMessage& GetMessageAt(FConsoleOutputHandle Handle, std::size_t Index);
}
