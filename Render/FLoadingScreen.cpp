#include "pch.h"

#include "FLoadingScreen.h"

#include "Renderer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <objbase.h>
#include <thread>

FLoadingProgress::FLoadingProgress()
    : mStatus{"Preparing"} {
}

FLoadingProgress::~FLoadingProgress() = default;

void FLoadingProgress::SetProgress(float Progress, const std::string& Status) {
    {
        const std::lock_guard<std::mutex> Lock{mStatusMutex};
        mStatus = Status;
    }

    mProgress.store(std::clamp(Progress, 0.0f, 1.0f), std::memory_order_release);
}

float FLoadingProgress::GetProgress() const {
    return mProgress.load(std::memory_order_acquire);
}

std::string FLoadingProgress::GetStatus() const {
    const std::lock_guard<std::mutex> Lock{mStatusMutex};
    return mStatus;
}

FLoadingScreen::FLoadingScreen() = default;

FLoadingScreen::~FLoadingScreen() = default;

bool FLoadingScreen::LoadLogo(ID3D11Device* Device) {
    if (Device == nullptr) {
        return false;
    }

    int Width{};
    int Height{};
    int Channels{};
    stbi_uc* Pixels{stbi_load("./Content/Macaw.png", &Width, &Height, &Channels, STBI_rgb_alpha)};
    if (Pixels == nullptr || Width <= 0 || Height <= 0) {
        stbi_image_free(Pixels);
        return false;
    }

    D3D11_TEXTURE2D_DESC TextureDescription{};
    TextureDescription.Width = static_cast<UINT>(Width);
    TextureDescription.Height = static_cast<UINT>(Height);
    TextureDescription.MipLevels = 1;
    TextureDescription.ArraySize = 1;
    TextureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDescription.SampleDesc.Count = 1;
    TextureDescription.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA TextureData{};
    TextureData.pSysMem = Pixels;
    TextureData.SysMemPitch = static_cast<UINT>(Width) * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture{};
    const HRESULT TextureResult{Device->CreateTexture2D(&TextureDescription, &TextureData, Texture.GetAddressOf())};
    stbi_image_free(Pixels);
    if (FAILED(TextureResult)) {
        return false;
    }

    if (FAILED(Device->CreateShaderResourceView(Texture.Get(), nullptr, mLogoShaderResourceView.GetAddressOf()))) {
        return false;
    }

    mLogoWidth = Width;
    mLogoHeight = Height;
    return true;
}

bool FLoadingScreen::Run(FRenderer& Renderer, HACCEL AcceleratorTable, const FLoadingTask& LoadingTask, const FLoadingFrameTask& LoadingFrameTask) {
    LoadLogo(Renderer.GetDevice());

    FLoadingProgress Progress{};
    std::atomic<bool> Finished{};
    std::atomic<bool> Succeeded{};

    std::thread LoadingThread{[&Progress, &LoadingTask, &Finished, &Succeeded]() {
        const HRESULT ComResult{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
        const bool UninitializeCom{SUCCEEDED(ComResult)};

        try {
            Succeeded.store(LoadingTask(Progress), std::memory_order_release);
        } catch (...) {
            Progress.SetProgress(Progress.GetProgress(), "Loading failed");
            Succeeded.store(false, std::memory_order_release);
        }

        if (UninitializeCom) {
            CoUninitialize();
        }

        Finished.store(true, std::memory_order_release);
    }};

    bool QuitRequested{};
    bool FrameTaskFinished{};

    while (true) {
        MSG Message{};
        while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) {
                QuitRequested = true;
                continue;
            }

            if (!TranslateAccelerator(Message.hwnd, AcceleratorTable, &Message)) {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
        }

        const bool LoadingFinished{Finished.load(std::memory_order_acquire)};
        if (!QuitRequested) {
            if (LoadingFinished && Succeeded.load(std::memory_order_acquire)) {
                try {
                    Renderer.BeginFrame(0.0f);
                    FrameTaskFinished = LoadingFrameTask(Progress);
                } catch (...) {
                    Progress.SetProgress(Progress.GetProgress(), "Loading failed");
                    Succeeded.store(false, std::memory_order_release);
                }
            }
            Render(Renderer, Progress);
        }

        if (LoadingFinished && (QuitRequested || !Succeeded.load(std::memory_order_acquire) || FrameTaskFinished)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{8});
    }

    LoadingThread.join();
    return !QuitRequested && Succeeded.load(std::memory_order_acquire);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FLoadingScreen::TakeLogoShaderResourceView() {
    return std::move(mLogoShaderResourceView);
}

void FLoadingScreen::Render(FRenderer& Renderer, const FLoadingProgress& Progress) {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImVec2 DisplaySize{ImGui::GetIO().DisplaySize};
    const ImGuiWindowFlags WindowFlags{ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus};

    ImGui::SetNextWindowPos(ImVec2{});
    ImGui::SetNextWindowSize(DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.025f, 0.035f, 0.055f, 1.0f});
    ImGui::Begin("##LoadingWindow", nullptr, WindowFlags);

    const float LoadingValue{Progress.GetProgress()};
    const std::string Status{Progress.GetStatus()};
    const float PanelWidth{std::clamp(DisplaySize.x - 96.0f, 240.0f, 640.0f)};
    const ImVec2 TitleSize{ImGui::CalcTextSize("MACAW ENGINE")};
    const ImVec2 StatusSize{ImGui::CalcTextSize(Status.c_str())};
    const float CenterY{DisplaySize.y * 0.5f};
    const bool HasLogo{mLogoShaderResourceView != nullptr && mLogoHeight > 0};
    const float LogoHeight{128.0f};
    const float LogoWidth{HasLogo ? LogoHeight * static_cast<float>(mLogoWidth) / static_cast<float>(mLogoHeight) : 0.0f};

    if (HasLogo) {
        ImGui::SetCursorPos(ImVec2{(DisplaySize.x - LogoWidth) * 0.5f, CenterY - 145.0f});
        ImGui::Image(ImTextureRef{reinterpret_cast<ImTextureID>(mLogoShaderResourceView.Get())}, ImVec2{LogoWidth, LogoHeight});
    }

    ImGui::SetCursorPos(ImVec2{(DisplaySize.x - TitleSize.x) * 0.5f, HasLogo ? CenterY + 5.0f : CenterY - 72.0f});
    ImGui::TextUnformatted("MACAW ENGINE");
    ImGui::SetCursorPos(ImVec2{(DisplaySize.x - StatusSize.x) * 0.5f, HasLogo ? CenterY + 43.0f : CenterY - 24.0f});
    ImGui::TextUnformatted(Status.c_str());
    ImGui::SetCursorPos(ImVec2{(DisplaySize.x - PanelWidth) * 0.5f, HasLogo ? CenterY + 79.0f : CenterY + 12.0f});

    char Percentage[16]{};
    sprintf_s(Percentage, "%.0f%%", LoadingValue * 100.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.12f, 0.55f, 0.92f, 1.0f});
    ImGui::ProgressBar(LoadingValue, ImVec2{PanelWidth, 24.0f}, Percentage);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::Render();
    Renderer.BeginUiRender();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    Renderer.EndFrame();

    auto& Io{ImGui::GetIO()};
    if (Io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
