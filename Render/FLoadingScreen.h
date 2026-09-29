#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "Render/Renderer.h"

class FLoadingProgress {
public:
    FLoadingProgress();
    ~FLoadingProgress();

public:
    void SetProgress(float Progress, const std::string& Status);
    float GetProgress() const;
    std::string GetStatus() const;

private:
    std::atomic<float> mProgress{};
    mutable std::mutex mStatusMutex{};
    std::string mStatus{};
};

class FLoadingScreen {
public:
    using FLoadingTask = std::function<bool(FLoadingProgress&)>;
    using FLoadingFrameTask = std::function<bool(FLoadingProgress&)>;

public:
    FLoadingScreen();
    ~FLoadingScreen();

public:
    bool Run(FRenderer& Renderer, HACCEL AcceleratorTable, const FLoadingTask& LoadingTask, const FLoadingFrameTask& LoadingFrameTask);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TakeLogoShaderResourceView();

private:
    bool LoadLogo(ID3D11Device* Device);
    void Render(FRenderer& Renderer, const FLoadingProgress& Progress);

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mLogoShaderResourceView{};
    int mLogoWidth{};
    int mLogoHeight{};
};
