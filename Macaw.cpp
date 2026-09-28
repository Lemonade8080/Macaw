#include "pch.h"

#include "Application/IApplication.h"
#ifdef OBJ_VIEWER
#include "Application/FViewApplication.h"
#else
#include "Application/FEditorApplication.h"
#endif

int APIENTRY wWinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PreviousInstance, _In_ LPWSTR CommandLine, _In_ int ShowCommand) {
    UNREFERENCED_PARAMETER(PreviousInstance);
    UNREFERENCED_PARAMETER(CommandLine);

#ifdef OBJ_VIEWER
    std::unique_ptr<IApplication> Application{std::make_unique<FViewApplication>()};
#else
    std::unique_ptr<IApplication> Application{std::make_unique<FEditorApplication>()};
#endif
    return Application->Run(Instance, ShowCommand);
}
