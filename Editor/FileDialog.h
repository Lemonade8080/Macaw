#pragma once

#include "Core/STL.h"
#include <filesystem>
#include <windows.h>

FString OpenFileDialog(HWND Owner, const std::filesystem::path& InitialDirectory, const char* Filter, const char* DefaultExtension = nullptr);
