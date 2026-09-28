#pragma once

#include <source_location>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ErrorHandler {
    enum class EErrorLevel {
        Warning,
        Error,
        Critical
    };

    void Report(std::string_view Title, std::string_view Message, EErrorLevel Level);
    void Report(bool BCondition, std::string_view Title, std::string_view Message, EErrorLevel Level);

#ifdef _WIN32
    void ReportHRESULT(HRESULT Result, std::string_view Title, std::string_view Message, EErrorLevel Level, const std::source_location& Location = std::source_location::current());
    void ReportWin32(DWORD ErrorCode, std::string_view Title, std::string_view Message, EErrorLevel Level, const std::source_location& Location = std::source_location::current());
#endif
}
