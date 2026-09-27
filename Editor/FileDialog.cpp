#include "pch.h"
#include "FileDialog.h"

#include <commdlg.h>

FString OpenFileDialog(HWND Owner, const std::filesystem::path& InitialDirectory, const char* Filter, const char* DefaultExtension) {
    const std::string DirectoryPath{std::filesystem::absolute(InitialDirectory).string()};
    if (!std::filesystem::exists(DirectoryPath)) {
        std::filesystem::create_directories(DirectoryPath);
    }

    char FileName[MAX_PATH]{};
    OPENFILENAMEA OpenFileName{};
    OpenFileName.lStructSize = sizeof(OpenFileName);
    OpenFileName.hwndOwner = Owner;
    OpenFileName.lpstrFilter = Filter;
    OpenFileName.lpstrFile = FileName;
    OpenFileName.nMaxFile = MAX_PATH;
    OpenFileName.lpstrInitialDir = DirectoryPath.c_str();
    OpenFileName.lpstrDefExt = DefaultExtension;
    OpenFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&OpenFileName) ? FString{FileName} : FString{};
}
