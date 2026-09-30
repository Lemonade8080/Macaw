#include "pch.h"

#include "FSceneRenderSurface.h"

#include "Core/Base/ErrorHandler.h"

void FSceneRenderSurface::InitializeSwapChain(ID3D11Device* Device, IDXGISwapChain* InSwapChain) {
    Reset();
    mStorageMode = EStorageMode::SwapChain;
    mSwapChain = InSwapChain;
    CreateSwapChainResources(Device);
}

void FSceneRenderSurface::InitializeOffscreen(ID3D11Device* Device, std::uint32_t Width, std::uint32_t Height) {
    Reset();
    mStorageMode = EStorageMode::Offscreen;
    Resize(Device, Width, Height);
}

bool FSceneRenderSurface::Resize(ID3D11Device* Device, std::uint32_t Width, std::uint32_t Height) {
    if (Device == nullptr || Width == 0 || Height == 0) {
        return false;
    }

    if (mStorageMode == EStorageMode::Offscreen && mViewport.Width == static_cast<float>(Width) && mViewport.Height == static_cast<float>(Height)) {
        return true;
    }

    ResetResources();
    if (mStorageMode == EStorageMode::SwapChain) {
        DXGI_SWAP_CHAIN_DESC SwapChainDescription{};
        ErrorHandler::ReportHRESULT(mSwapChain->GetDesc(&SwapChainDescription), "[ FSceneRenderSurface ]", "Failed to get swap chain description.", ErrorHandler::EErrorLevel::Critical);
        ErrorHandler::ReportHRESULT(mSwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, SwapChainDescription.Flags), "[ FSceneRenderSurface ]", "Failed to resize swap chain buffers.", ErrorHandler::EErrorLevel::Critical);
        CreateSwapChainResources(Device);
    } else if (mStorageMode == EStorageMode::Offscreen) {
        CreateOffscreenResources(Device, Width, Height);
    }

    return IsValid();
}

void FSceneRenderSurface::Bind(ID3D11DeviceContext* Context) const {
    if (!IsValid()) {
        return;
    }

    ID3D11RenderTargetView* TargetView{mRenderTargetView.Get()};
    Context->OMSetRenderTargets(1, &TargetView, mDepthStencilView.Get());
    Context->RSSetViewports(1, &mViewport);
}

void FSceneRenderSurface::Clear(ID3D11DeviceContext* Context, const float ClearColor[4]) const {
    if (!IsValid()) {
        return;
    }

    Context->ClearRenderTargetView(mRenderTargetView.Get(), ClearColor);
    Context->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void FSceneRenderSurface::ClearDepth(ID3D11DeviceContext* Context) const {
    if (!IsValid()) {
        return;
    }

    Context->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void FSceneRenderSurface::Reset() {
    ResetResources();
    mSwapChain.Reset();
    mStorageMode = EStorageMode::None;
}

bool FSceneRenderSurface::IsValid() const {
    return mRenderTargetView != nullptr && mDepthStencilView != nullptr && mDepthShaderResourceView != nullptr && mViewport.Width > 0.0f && mViewport.Height > 0.0f;
}

const D3D11_VIEWPORT& FSceneRenderSurface::GetViewport() const {
    return mViewport;
}

ID3D11ShaderResourceView* FSceneRenderSurface::GetShaderResourceView() const {
    return mShaderResourceView.Get();
}

ID3D11ShaderResourceView* FSceneRenderSurface::GetDepthShaderResourceView() const {
    return mDepthShaderResourceView.Get();
}

void FSceneRenderSurface::CreateSwapChainResources(ID3D11Device* Device) {
    DXGI_SWAP_CHAIN_DESC SwapChainDescription{};
    ErrorHandler::ReportHRESULT(mSwapChain->GetDesc(&SwapChainDescription), "[ FSceneRenderSurface ]", "Failed to get swap chain description.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::ReportHRESULT(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mColorTexture.GetAddressOf())), "[ FSceneRenderSurface ]", "Failed to get swap chain back buffer.", ErrorHandler::EErrorLevel::Critical);

    D3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDescription{};
    RenderTargetViewDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    RenderTargetViewDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ErrorHandler::ReportHRESULT(Device->CreateRenderTargetView(mColorTexture.Get(), &RenderTargetViewDescription, mRenderTargetView.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create swap chain render target view.", ErrorHandler::EErrorLevel::Critical);

    CreateDepthStencilResources(Device, SwapChainDescription.BufferDesc.Width, SwapChainDescription.BufferDesc.Height);
}

void FSceneRenderSurface::CreateOffscreenResources(ID3D11Device* Device, std::uint32_t Width, std::uint32_t Height) {
    D3D11_TEXTURE2D_DESC TextureDescription{};
    TextureDescription.Width = Width;
    TextureDescription.Height = Height;
    TextureDescription.MipLevels = 1;
    TextureDescription.ArraySize = 1;
    TextureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDescription.SampleDesc.Count = 1;
    TextureDescription.Usage = D3D11_USAGE_DEFAULT;
    TextureDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ErrorHandler::ReportHRESULT(Device->CreateTexture2D(&TextureDescription, nullptr, mColorTexture.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create scene color texture.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::ReportHRESULT(Device->CreateRenderTargetView(mColorTexture.Get(), nullptr, mRenderTargetView.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create scene render target view.", ErrorHandler::EErrorLevel::Critical);
    ErrorHandler::ReportHRESULT(Device->CreateShaderResourceView(mColorTexture.Get(), nullptr, mShaderResourceView.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create scene shader resource view.", ErrorHandler::EErrorLevel::Critical);
    CreateDepthStencilResources(Device, Width, Height);
}

void FSceneRenderSurface::CreateDepthStencilResources(ID3D11Device* Device, std::uint32_t Width, std::uint32_t Height) {
    D3D11_TEXTURE2D_DESC TextureDescription{};
    TextureDescription.Width = Width;
    TextureDescription.Height = Height;
    TextureDescription.MipLevels = 1;
    TextureDescription.ArraySize = 1;
    TextureDescription.Format = DXGI_FORMAT_R24G8_TYPELESS;
    TextureDescription.SampleDesc.Count = 1;
    TextureDescription.Usage = D3D11_USAGE_DEFAULT;
    TextureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    ErrorHandler::ReportHRESULT(Device->CreateTexture2D(&TextureDescription, nullptr, mDepthStencilTexture.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create depth stencil texture.", ErrorHandler::EErrorLevel::Critical);

    D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDescription{};
    DepthStencilViewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthStencilViewDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ErrorHandler::ReportHRESULT(Device->CreateDepthStencilView(mDepthStencilTexture.Get(), &DepthStencilViewDescription, mDepthStencilView.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create depth stencil view.", ErrorHandler::EErrorLevel::Critical);
    D3D11_SHADER_RESOURCE_VIEW_DESC DepthResourceDescription{};
    DepthResourceDescription.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    DepthResourceDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    DepthResourceDescription.Texture2D.MipLevels = 1;
    ErrorHandler::ReportHRESULT(Device->CreateShaderResourceView(mDepthStencilTexture.Get(), &DepthResourceDescription, mDepthShaderResourceView.GetAddressOf()), "[ FSceneRenderSurface ]", "Failed to create depth shader resource view.", ErrorHandler::EErrorLevel::Critical);
    mViewport = {0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f};
}

void FSceneRenderSurface::ResetResources() {
    mDepthShaderResourceView.Reset();
    mDepthStencilView.Reset();
    mDepthStencilTexture.Reset();
    mShaderResourceView.Reset();
    mRenderTargetView.Reset();
    mColorTexture.Reset();
    mViewport = {};
}
