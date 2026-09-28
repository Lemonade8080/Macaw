#include "pch.h"
#include "FApplication.h"
#include "Core/Stat/Stat.h"

#include <cstdio>

#include "Resource.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

namespace {
    constexpr UINT DefaultWindowWidth{1920};
    constexpr UINT DefaultWindowHeight{1080};
    constexpr int TitleBarHeight{26};
    constexpr int CaptionButtonWidth{46};
    constexpr int ResizeBorderWidth{7};
    constexpr int MenuStartX{50};
    constexpr wchar_t WindowTitle[]{L"Macaw Engine"};
    constexpr wchar_t WindowClass[]{L"MacawEngineClass"};
    constexpr wchar_t ExternalDropOriginalWndProcProperty[]{L"Macaw.ExternalDropOriginalWndProc"};
    constexpr wchar_t ExternalDropApplicationProperty[]{L"Macaw.ExternalDropApplication"};
}

void FApplication::QueueExternalFileDrops(HWND WindowHandle, HDROP DropHandle) {
    POINT DropPosition{};
    DragQueryPoint(DropHandle, &DropPosition);
    ClientToScreen(WindowHandle, &DropPosition);

    const UINT FileCount{DragQueryFileW(DropHandle, 0xFFFFFFFF, nullptr, 0)};
    for (UINT FileIndex{0}; FileIndex < FileCount; ++FileIndex) {
        const UINT CharacterCount{DragQueryFileW(DropHandle, FileIndex, nullptr, 0)};
        std::wstring FilePath{};
        FilePath.resize(static_cast<std::size_t>(CharacterCount + 1), L'\0');
        DragQueryFileW(DropHandle, FileIndex, FilePath.data(), CharacterCount + 1);
        FilePath.resize(CharacterCount);
        mPendingExternalFileDrops.push_back({std::filesystem::path{FilePath}, DropPosition});
    }

    DragFinish(DropHandle);
}

LRESULT CALLBACK FApplication::ExternalDropWindowProcedure(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam) {
    const WNDPROC OriginalWndProc{reinterpret_cast<WNDPROC>(GetPropW(WindowHandle, ExternalDropOriginalWndProcProperty))};

    FApplication* Application{reinterpret_cast<FApplication*>(GetPropW(WindowHandle, ExternalDropApplicationProperty))};

    if (Message == WM_DROPFILES && Application != nullptr) {
        Application->QueueExternalFileDrops(WindowHandle, reinterpret_cast<HDROP>(WParam));
        return 0;
    }

    if (Message == WM_NCDESTROY) {
        SetWindowLongPtrW(WindowHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OriginalWndProc));
        RemovePropW(WindowHandle, ExternalDropOriginalWndProcProperty);
        RemovePropW(WindowHandle, ExternalDropApplicationProperty);
    }

    return OriginalWndProc != nullptr ? CallWindowProcW(OriginalWndProc, WindowHandle, Message, WParam, LParam) : DefWindowProcW(WindowHandle, Message, WParam, LParam);
}

void FApplication::EnableExternalDropsForImGuiViewports() {
    for (ImGuiViewport* Viewport : ImGui::GetPlatformIO().Viewports) {
        HWND ViewportWindow{static_cast<HWND>(Viewport->PlatformHandle)};
        if (ViewportWindow == nullptr) {
            continue;
        }

        DragAcceptFiles(ViewportWindow, TRUE);

        if (ViewportWindow == mWindowState.mWindowHandle || GetPropW(ViewportWindow, ExternalDropOriginalWndProcProperty) != nullptr) {
            continue;
        }

        if (!SetPropW(ViewportWindow, ExternalDropApplicationProperty, reinterpret_cast<HANDLE>(this))) {
            continue;
        }
        const WNDPROC OriginalWndProc{reinterpret_cast<WNDPROC>(SetWindowLongPtrW(ViewportWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ExternalDropWindowProcedure)))};
        if (OriginalWndProc != nullptr) {
            if (!SetPropW(ViewportWindow, ExternalDropOriginalWndProcProperty, reinterpret_cast<HANDLE>(OriginalWndProc))) {
                SetWindowLongPtrW(ViewportWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OriginalWndProc));
                RemovePropW(ViewportWindow, ExternalDropApplicationProperty);
            }
        }
    }
}

void FApplication::RestoreGameWindow() {
    const DWORD Style{WS_OVERLAPPEDWINDOW};
    const DWORD ExtendedStyle{WS_EX_APPWINDOW};
    MONITORINFO MonitorInformation{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(mWindowState.mWindowHandle, MONITOR_DEFAULTTONEAREST), &MonitorInformation);
    const RECT WorkArea{MonitorInformation.rcWork};
    const int WindowWidth{std::min(static_cast<int>(DefaultWindowWidth), static_cast<int>(WorkArea.right - WorkArea.left))};
    const int WindowHeight{std::min(static_cast<int>(DefaultWindowHeight), static_cast<int>(WorkArea.bottom - WorkArea.top))};
    const int PositionX{WorkArea.left + (WorkArea.right - WorkArea.left - WindowWidth) / 2};
    const int PositionY{WorkArea.top + (WorkArea.bottom - WorkArea.top - WindowHeight) / 2};

    SetWindowLongPtrW(mWindowState.mWindowHandle, GWL_STYLE, static_cast<LONG_PTR>(Style));
    SetWindowLongPtrW(mWindowState.mWindowHandle, GWL_EXSTYLE, static_cast<LONG_PTR>(ExtendedStyle));
    SetWindowPos(mWindowState.mWindowHandle, nullptr, PositionX, PositionY, WindowWidth, WindowHeight, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
    DragAcceptFiles(mWindowState.mWindowHandle, TRUE);
}

void FApplication::DrawCaptionButton(const char* Identifier, int Index, UINT Command) {
    const ImVec2 WindowPosition{ImGui::GetWindowPos()};
    const float ButtonX{WindowPosition.x + ImGui::GetWindowWidth() - static_cast<float>((3 - Index) * CaptionButtonWidth)};
    const ImVec2 ButtonPosition{ButtonX, WindowPosition.y};
    const ImVec2 ButtonSize{static_cast<float>(CaptionButtonWidth), static_cast<float>(TitleBarHeight)};
    ImGui::SetCursorScreenPos(ButtonPosition);
    const bool Pressed{ImGui::InvisibleButton(Identifier, ButtonSize)};
    const bool Hovered{ImGui::IsItemHovered()};
    const bool Held{ImGui::IsItemActive()};
    ImDrawList* DrawList{ImGui::GetWindowDrawList()};
    const ImU32 BackgroundColor{Index == 2 ? IM_COL32(192, 55, 55, 255) : IM_COL32(66, 68, 73, 255)};
    if (Hovered || Held) {
        DrawList->AddRectFilled(ButtonPosition, ImVec2{ButtonPosition.x + ButtonSize.x, ButtonPosition.y + ButtonSize.y}, BackgroundColor);
    }

    const float CenterX{ButtonPosition.x + ButtonSize.x * 0.5f};
    const float CenterY{ButtonPosition.y + ButtonSize.y * 0.5f};
    const ImU32 IconColor{IM_COL32(225, 228, 232, 255)};
    if (Index == 0) {
        DrawList->AddLine(ImVec2{CenterX - 6.0f, CenterY + 4.0f}, ImVec2{CenterX + 6.0f, CenterY + 4.0f}, IconColor, 1.5f);
    } else if (Index == 1 && IsZoomed(mWindowState.mWindowHandle)) {
        DrawList->AddRect(ImVec2{CenterX - 4.0f, CenterY - 3.0f}, ImVec2{CenterX + 6.0f, CenterY + 5.0f}, IconColor, 0.0f, 0, 1.5f);
        DrawList->AddLine(ImVec2{CenterX - 6.0f, CenterY + 2.0f}, ImVec2{CenterX - 6.0f, CenterY - 5.0f}, IconColor, 1.5f);
        DrawList->AddLine(ImVec2{CenterX - 6.0f, CenterY - 5.0f}, ImVec2{CenterX + 3.0f, CenterY - 5.0f}, IconColor, 1.5f);
    } else if (Index == 1) {
        DrawList->AddRect(ImVec2{CenterX - 6.0f, CenterY - 5.0f}, ImVec2{CenterX + 6.0f, CenterY + 5.0f}, IconColor, 0.0f, 0, 1.5f);
    } else {
        DrawList->AddLine(ImVec2{CenterX - 5.0f, CenterY - 5.0f}, ImVec2{CenterX + 5.0f, CenterY + 5.0f}, IconColor, 1.5f);
        DrawList->AddLine(ImVec2{CenterX + 5.0f, CenterY - 5.0f}, ImVec2{CenterX - 5.0f, CenterY + 5.0f}, IconColor, 1.5f);
    }

    if (Pressed) {
        PostMessageW(mWindowState.mWindowHandle, WM_SYSCOMMAND, Command, 0);
    }
}

void FApplication::DrawTitleBar() {
    const float MenuVerticalPadding{std::max(0.0f, (static_cast<float>(TitleBarHeight) - ImGui::GetFontSize()) * 0.5f)};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.0f, MenuVerticalPadding});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(31, 32, 36, 255));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, IM_COL32(31, 32, 36, 255));
    const ImGuiWindowFlags Flags{ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus};
    if (ImGui::BeginViewportSideBar("##EditorTitleBar", ImGui::GetMainViewport(), ImGuiDir_Up, static_cast<float>(TitleBarHeight), Flags)) {
        ImDrawList* DrawList{ImGui::GetWindowDrawList()};
        const ImVec2 Position{ImGui::GetWindowPos()};
        const float Width{ImGui::GetWindowWidth()};
        const float FpsX{Position.x + Width - static_cast<float>(3 * CaptionButtonWidth) - 110.0f};
        DrawList->AddLine(ImVec2{Position.x, Position.y + static_cast<float>(TitleBarHeight) - 1.0f}, ImVec2{Position.x + Width, Position.y + static_cast<float>(TitleBarHeight) - 1.0f}, IM_COL32(72, 74, 78, 255));
        if (ImGui::BeginMenuBar()) {
            if (mEditorLogo.Get() != nullptr) {
                DrawList->AddImage(reinterpret_cast<ImTextureID>(mEditorLogo.Get()), ImVec2{Position.x + 11.0f, Position.y + 1.0f}, ImVec2{Position.x + 27.0f, Position.y + static_cast<float>(TitleBarHeight) - 1.0f}, ImVec2{0.23f, 0.11f}, ImVec2{0.77f, 0.90f});
            }
            ImGui::SetCursorPosX(static_cast<float>(MenuStartX));
            if (mContext.mMenuPanel != nullptr) {
                mContext.mMenuPanel->DrawPanel();
            }
            mWindowState.mMenuHitRight = static_cast<int>(ImGui::GetCursorPosX()) + 10;
            if (static_cast<float>(mWindowState.mMenuHitRight) + 112.0f < FpsX - Position.x) {
                char FpsText[32]{};
                std::snprintf(FpsText, sizeof(FpsText), "FPS: %.1f", Stat::GetFrameStats().mFramesPerSecond);
                DrawList->AddText(ImVec2{FpsX, Position.y + 7.0f}, IM_COL32(152, 156, 163, 255), FpsText);
            }
            DrawCaptionButton("##MinimizeWindow", 0, SC_MINIMIZE);
            DrawCaptionButton("##MaximizeWindow", 1, IsZoomed(mWindowState.mWindowHandle) ? SC_RESTORE : SC_MAXIMIZE);
            DrawCaptionButton("##CloseWindow", 2, SC_CLOSE);
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

ATOM FApplication::RegisterWindowClass(HINSTANCE HInstance) {
    WNDCLASSEXW Wcex{};

    Wcex.cbSize = sizeof(WNDCLASSEX);

    Wcex.style = CS_HREDRAW | CS_VREDRAW;
    Wcex.lpfnWndProc = WindowProcedure;
    Wcex.cbClsExtra = 0;
    Wcex.cbWndExtra = 0;
    Wcex.hInstance = HInstance;
    Wcex.hIcon = LoadIcon(HInstance, MAKEINTRESOURCE(IDI_MACAW));
    Wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    Wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    Wcex.lpszClassName = WindowClass;
    Wcex.hIconSm = LoadIcon(Wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&Wcex);
}

bool FApplication::CreateApplicationWindow(HINSTANCE HInstance, int ShowCommand) {
    const DWORD Style{WS_POPUP};
    const DWORD ExtendedStyle{WS_EX_APPWINDOW};
    const int PositionX{(GetSystemMetrics(SM_CXSCREEN) - static_cast<int>(mLoadingWindowWidth)) / 2};
    const int PositionY{(GetSystemMetrics(SM_CYSCREEN) - static_cast<int>(mLoadingWindowHeight)) / 2};

    mWindowState.mWindowHandle = CreateWindowExW(ExtendedStyle, WindowClass, WindowTitle, Style, PositionX, PositionY, static_cast<int>(mLoadingWindowWidth), static_cast<int>(mLoadingWindowHeight), nullptr, nullptr, HInstance, this);

    if (mWindowState.mWindowHandle == nullptr) {
        const DWORD ErrorCode{GetLastError()};
        OutputDebugStringA(("Window Creation Failed! Error Code: " + std::to_string(ErrorCode) + "\n").c_str());
        return FALSE;
    }

    ShowWindow(mWindowState.mWindowHandle, ShowCommand);
    UpdateWindow(mWindowState.mWindowHandle);
    DragAcceptFiles(mWindowState.mWindowHandle, TRUE);
    return TRUE;
}

LRESULT CALLBACK FApplication::WindowProcedure(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam) {
    FApplication* Application{reinterpret_cast<FApplication*>(GetWindowLongPtrW(WindowHandle, GWLP_USERDATA))};
    if (Message == WM_NCCREATE) {
        const CREATESTRUCTW* Creation{reinterpret_cast<const CREATESTRUCTW*>(LParam)};
        Application = static_cast<FApplication*>(Creation->lpCreateParams);
        SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Application));
    }
    if (Application == nullptr) {
        return DefWindowProcW(WindowHandle, Message, WParam, LParam);
    }
    const LRESULT Result{Application->ProcessWindowMessage(WindowHandle, Message, WParam, LParam)};
    if (Message == WM_NCDESTROY) {
        SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, 0);
        Application->mWindowState.mWindowHandle = nullptr;
    }
    return Result;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);

LRESULT FApplication::ProcessWindowMessage(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam) {
    if (const auto Result{ImGui_ImplWin32_WndProcHandler(WindowHandle, Message, WParam, LParam)}) {
        return Result;
    }

    if (mAcceptGameInput.load(std::memory_order_acquire)) {
        mContext.mMouseInput.ProcessWindowMessage(Message, WParam, LParam);
        mContext.mKeyboardInput.ProcessWindowMessage(Message, WParam, LParam);
    }

    switch (Message) {
        case WM_ENTERSIZEMOVE: {
            mWindowState.mInMoveLoop = true;
            return 0;
        }
        case WM_EXITSIZEMOVE: {
            mWindowState.mInMoveLoop = false;
            if (mWindowState.mFrameEnabled) {
                RenderFrame();
            }
            return 0;
        }
        case WM_WINDOWPOSCHANGED: {
            const WINDOWPOS* WindowPosition{reinterpret_cast<const WINDOWPOS*>(LParam)};
            const bool SizeChanged{(WindowPosition->flags & SWP_NOSIZE) == 0};
            const LRESULT Result{DefWindowProcW(WindowHandle, Message, WParam, LParam)};
            if (mWindowState.mInMoveLoop && SizeChanged && mWindowState.mFrameEnabled) {
                RenderFrame();
            }
            return Result;
        }
        case WM_NCCALCSIZE: {
            if (WParam != 0 && IsZoomed(WindowHandle)) {
                NCCALCSIZE_PARAMS* SizeParameters{reinterpret_cast<NCCALCSIZE_PARAMS*>(LParam)};
                const HMONITOR Monitor{MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST)};
                MONITORINFO MonitorInformation{sizeof(MONITORINFO)};
                if (GetMonitorInfoW(Monitor, &MonitorInformation)) {
                    SizeParameters->rgrc[0] = MonitorInformation.rcWork;
                }
            }
            return 0;
        }
        case WM_NCHITTEST: {
            RECT WindowRectangle{};
            GetWindowRect(WindowHandle, &WindowRectangle);
            const POINT Cursor{static_cast<SHORT>(LOWORD(LParam)), static_cast<SHORT>(HIWORD(LParam))};
            const int Border{MulDiv(ResizeBorderWidth, static_cast<int>(GetDpiForWindow(WindowHandle)), 96)};
            const bool Left{Cursor.x < WindowRectangle.left + Border};
            const bool Right{Cursor.x >= WindowRectangle.right - Border};
            const bool Top{Cursor.y < WindowRectangle.top + Border};
            const bool Bottom{Cursor.y >= WindowRectangle.bottom - Border};

            if (!IsZoomed(WindowHandle)) {
                if (Top && Left) {
                    return HTTOPLEFT;
                }
                if (Top && Right) {
                    return HTTOPRIGHT;
                }
                if (Bottom && Left) {
                    return HTBOTTOMLEFT;
                }
                if (Bottom && Right) {
                    return HTBOTTOMRIGHT;
                }
                if (Left) {
                    return HTLEFT;
                }
                if (Right) {
                    return HTRIGHT;
                }
                if (Top) {
                    return HTTOP;
                }
                if (Bottom) {
                    return HTBOTTOM;
                }
            }

            if (Cursor.y < WindowRectangle.top + TitleBarHeight && (Cursor.x < WindowRectangle.left + MenuStartX || Cursor.x >= WindowRectangle.left + mWindowState.mMenuHitRight) && Cursor.x < WindowRectangle.right - 3 * CaptionButtonWidth) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_GETMINMAXINFO: {
            const HMONITOR Monitor{MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST)};
            MONITORINFO MonitorInformation{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(Monitor, &MonitorInformation)) {
                MINMAXINFO* SizeInformation{reinterpret_cast<MINMAXINFO*>(LParam)};
                SizeInformation->ptMaxPosition = POINT{MonitorInformation.rcWork.left - MonitorInformation.rcMonitor.left, MonitorInformation.rcWork.top - MonitorInformation.rcMonitor.top};
                SizeInformation->ptMaxSize = POINT{MonitorInformation.rcWork.right - MonitorInformation.rcWork.left, MonitorInformation.rcWork.bottom - MonitorInformation.rcWork.top};
                SizeInformation->ptMinTrackSize = POINT{520, 360};
            }
            return 0;
        }
        case WM_DROPFILES: {
            const HDROP DropHandle{reinterpret_cast<HDROP>(WParam)};
            QueueExternalFileDrops(WindowHandle, DropHandle);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_SIZE:
            if (WParam != SIZE_MINIMIZED) {
                Uint32 Width{LOWORD(LParam)};
                Uint32 Height{HIWORD(LParam)};
                mContext.mRenderer.ReSize(Width, Height);
            }
            break;
        default:
            return DefWindowProc(WindowHandle, Message, WParam, LParam);
    }
    return 0;
}
