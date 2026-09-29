#include "pch.h"

#include <cwchar>
#include <filesystem>
#include <nvapi/nvapi.h>

#include "Application/IApplication.h"
#ifdef OBJ_VIEWER
#include "Application/FViewApplication.h"
#else
#include "Application/FEditorApplication.h"
#endif

static NvAPI_Status ApplyNvidiaLoadBalance(NvDRSSessionHandle Session) {
    wchar_t ExecutablePath[MAX_PATH]{};
    const DWORD Length{GetModuleFileNameW(nullptr, ExecutablePath, MAX_PATH)};
    if (Length == 0 || Length >= MAX_PATH) {
        return NVAPI_ERROR;
    }
    const std::wstring ExecutableName{std::filesystem::path{ExecutablePath}.filename().wstring()};
    NvAPI_UnicodeString ApplicationName{};
    wcscpy_s(reinterpret_cast<wchar_t*>(ApplicationName), NVAPI_UNICODE_STRING_MAX, ExecutableName.c_str());

    NvDRSProfileHandle Profile{};
    NVDRS_APPLICATION Application{};
    Application.version = NVDRS_APPLICATION_VER;
    NvAPI_Status Status{NvAPI_DRS_FindApplicationByName(Session, ApplicationName, &Profile, &Application)};
    if (Status == NVAPI_EXECUTABLE_NOT_FOUND) {
        const std::wstring ProfileName{ExecutableName + L" - Load Balance"};
        NVDRS_PROFILE Information{};
        Information.version = NVDRS_PROFILE_VER;
        wcscpy_s(reinterpret_cast<wchar_t*>(Information.profileName), NVAPI_UNICODE_STRING_MAX, ProfileName.c_str());
        Status = NvAPI_DRS_CreateProfile(Session, &Information, &Profile);
        if (Status != NVAPI_OK) {
            return Status;
        }
        wcscpy_s(reinterpret_cast<wchar_t*>(Application.appName), NVAPI_UNICODE_STRING_MAX, ExecutableName.c_str());
        Status = NvAPI_DRS_CreateApplication(Session, Profile, &Application);
    }
    if (Status != NVAPI_OK) {
        return Status;
    }

    constexpr NvU32 SettingId{0x008F14F5};
    NVDRS_SETTING Current{};
    Current.version = NVDRS_SETTING_VER;
    Status = NvAPI_DRS_GetSetting(Session, Profile, SettingId, &Current);
    if (Status == NVAPI_OK && Current.settingType == NVDRS_DWORD_TYPE && Current.settingLocation == NVDRS_CURRENT_PROFILE_LOCATION && Current.u32CurrentValue == 1) {
        return NVAPI_OK;
    }
    if (Status != NVAPI_OK && Status != NVAPI_SETTING_NOT_FOUND) {
        return Status;
    }

    NVDRS_PROFILE Information{};
    Information.version = NVDRS_PROFILE_VER;
    Status = NvAPI_DRS_GetProfileInfo(Session, Profile, &Information);
    if (Status != NVAPI_OK) {
        return Status;
    }
    if (Information.numOfApps != 1) {
        return NVAPI_NOT_SUPPORTED;
    }

    NVDRS_SETTING Setting{};
    Setting.version = NVDRS_SETTING_VER;
    Setting.settingId = SettingId;
    Setting.settingType = NVDRS_DWORD_TYPE;
    Setting.u32CurrentValue = 1;
    Status = NvAPI_DRS_SetSetting(Session, Profile, &Setting);
    if (Status != NVAPI_OK) {
        return Status;
    }
    return NvAPI_DRS_SaveSettings(Session);
}

static NvAPI_Status ConfigureNvidiaLoadBalance() {
    NvAPI_Status Status{NvAPI_Initialize()};
    if (Status != NVAPI_OK) {
        return Status;
    }

    NvDRSSessionHandle Session{};
    Status = NvAPI_DRS_CreateSession(&Session);
    if (Status == NVAPI_OK) {
        Status = NvAPI_DRS_LoadSettings(Session);
        if (Status == NVAPI_OK) {
            Status = ApplyNvidiaLoadBalance(Session);
        }
        NvAPI_DRS_DestroySession(Session);
    }
    NvAPI_Unload();
    return Status;
}

int APIENTRY wWinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PreviousInstance, _In_ LPWSTR CommandLine, _In_ int ShowCommand) {
    const NvAPI_Status NvidiaStatus{ConfigureNvidiaLoadBalance()};
    if (NvidiaStatus != NVAPI_OK) {
        wchar_t Message[128]{};
        swprintf_s(Message, L"NVIDIA Load Balance configuration failed: %d\n", static_cast<int>(NvidiaStatus));
        OutputDebugStringW(Message);
    }

    UNREFERENCED_PARAMETER(PreviousInstance);
    UNREFERENCED_PARAMETER(CommandLine);

#ifdef OBJ_VIEWER
    std::unique_ptr<IApplication> Application{std::make_unique<FViewApplication>()};
#else
    std::unique_ptr<IApplication> Application{std::make_unique<FEditorApplication>()};
#endif
    return Application->Run(Instance, ShowCommand);
}
