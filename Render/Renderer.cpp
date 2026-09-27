#include "pch.h"

#include "Renderer.h"
#include "Core/Base/ErrorHandler.h"

#include <ranges>
#include <cmath>

FRenderer::~FRenderer() = default;

void FRenderer::Create(HWND WindowHandle, UINT Width, UINT Height) {
    mBackBufferWidth = Width;
    mBackBufferHeight = Height;

    FRenderer::CreateDeviceAndSwapChain(WindowHandle);
    auto BackBuffer{std::make_unique<FSceneRenderSurface>()};
    BackBuffer->InitializeSwapChain(mDevice.Get(), mSwapChain.Get());
    mBackBufferSurface = std::move(BackBuffer);

#ifdef _DEBUG
    mDevice.As(&mDebugInterface);
#endif
}

bool FRenderer::Initialize() {
    if (!CreateSamplerStates() || !mMeshRenderer.Initialize(mDevice.Get(), mDeviceContext.Get()) || !mLightContextArray.Initialize(mDevice.Get(), mDeviceContext.Get(), 16) || !mTextRenderer.Initialize(mDevice.Get(), 256) || !mBillboardRenderer.Initialize(mDevice.Get(), 64)) {
        return false;
    }

    mLineRenderer.Initialize(mDevice.Get());
    return true;
}

void FRenderer::BeginUiRender() {
    mBackBufferSurface->Bind(mDeviceContext.Get());
    mBackBufferSurface->Clear(mDeviceContext.Get(), mUiClearColor);
}

void FRenderer::BindSamplerStates() {
    std::array<ID3D11SamplerState*, 6> RawSamplerStates{};
    std::ranges::transform(mSamplerStates, RawSamplerStates.begin(), [](const auto& Sampler) {
        return Sampler.Get();
    });
    mDeviceContext->PSSetSamplers(0, static_cast<UINT>(RawSamplerStates.size()), RawSamplerStates.data());
    mDeviceContext->VSSetSamplers(0, static_cast<UINT>(RawSamplerStates.size()), RawSamplerStates.data());
}

void FRenderer::EndFrame() {
    mSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

ID3D11Device* FRenderer::GetDevice() const {
    return mDevice.Get();
}

ID3D11DeviceContext* FRenderer::GetDeviceContext() const {
    return mDeviceContext.Get();
}

void FRenderer::BindAssetRegistry(IRenderAssetRegistry* InAssetRegistry) {
    mAssetRegistry = InAssetRegistry;
}

void FRenderer::BeginFrame(float DeltaTime) {
    if (std::isfinite(DeltaTime) && DeltaTime > 0.0f) {
        mAnimationTime = std::fmod(mAnimationTime + DeltaTime, 25.0f);
        mAnimationFrame = static_cast<Uint32>(mAnimationTime / 0.1f) % 250;
    }
}

void FRenderer::RenderView(const FRenderView& View, const FRenderProbe& Probe) {
    if (View.mTarget == nullptr || !View.mTarget->IsValid() || mDeviceContext == nullptr) {
        return;
    }
    ID3D11ShaderResourceView* NullResource{nullptr};
    mDeviceContext->PSSetShaderResources(0, 1, &NullResource);
    View.mTarget->Bind(mDeviceContext.Get());
    const float ClearColor[]{View.mSettings.mClearColor.mX, View.mSettings.mClearColor.mY, View.mSettings.mClearColor.mZ, View.mSettings.mClearColor.mW};
    View.mTarget->Clear(mDeviceContext.Get(), ClearColor);
    if (mAssetRegistry == nullptr || !UploadLightContext(Probe)) {
        return;
    }
    mAssetRegistry->FlushMaterialBuffer(mDeviceContext.Get());
    mRenderQueue.Build(mAssetRegistry, View, Probe);
    const FRenderContext Context{mDevice.Get(), mDeviceContext.Get(), mAssetRegistry, mAssetRegistry->GetMaterialBufferSRV(), *mLightContextArray.GetSRV(), mFrameLightCount, mAnimationFrame};
    constexpr std::array Passes{ERenderPass::SceneGeometry, ERenderPass::SelectionOutline, ERenderPass::SceneGuides, ERenderPass::Gizmo, ERenderPass::Text, ERenderPass::Billboard, ERenderPass::OrientationAxis};
    for (const ERenderPass Pass : Passes) {
        if (View.IsPassEnabled(Pass)) {
            ExecutePass(Pass, Context, View, Probe);
        }
    }
    View.mTarget->Bind(mDeviceContext.Get());
}

bool FRenderer::UploadLightContext(const FRenderProbe& Probe) {
    ID3D11ShaderResourceView* NullResource{nullptr};
    mDeviceContext->PSSetShaderResources(2, 1, &NullResource);
    if (!mLightContextArray.UploadDiscard(mDevice.Get(), mDeviceContext.Get(), Probe.mLightProbes)) {
        return false;
    }
    mFrameLightCount = mLightContextArray.GetCount();
    return true;
}

void FRenderer::ExecutePass(ERenderPass Pass, const FRenderContext& Context, const FRenderView& View, const FRenderProbe& Probe) {
    View.mTarget->Bind(mDeviceContext.Get());
    BindSamplerStates();
    switch (Pass) {
        case ERenderPass::SceneGeometry:
            mMeshRenderer.Draw(Context, mRenderQueue.GetItems(Pass), View.mCamera, View.mRenderMode);
            break;
        case ERenderPass::SelectionOutline:
            mMeshRenderer.Draw(Context, mRenderQueue.GetItems(Pass), View.mCamera, ERenderMode::Outline);
            break;
        case ERenderPass::SceneGuides:
            DrawSceneGuides(View, Probe);
            break;
        case ERenderPass::Gizmo:
            if (!Probe.mGizmoProbes.empty()) {
                View.mTarget->ClearDepth(mDeviceContext.Get());
                mMeshRenderer.Draw(Context, mRenderQueue.GetItems(Pass), View.mCamera, ERenderMode::Lit);
            }
            break;
        case ERenderPass::Text:
            mTextRenderer.Render(mDeviceContext.Get(), Probe.mTextProbes, View.mCamera, mAssetRegistry);
            break;
        case ERenderPass::Billboard:
            mBillboardRenderer.Render(mDeviceContext.Get(), Probe.mBillboardProbes, View.mCamera, mAssetRegistry, View.mRenderMode);
            break;
        case ERenderPass::OrientationAxis:
            DrawOrientationAxis(View);
            break;
        default:
            break;
    }
}

void FRenderer::DrawSceneGuides(const FRenderView& View, const FRenderProbe& Probe) {
    mLineRenderer.Clear();
    for (const FLineProbe& Line : Probe.mSceneGuides.GetLines()) {
        mLineRenderer.AddGridLine(Line.mStart, Line.mEnd, Line.mColor, Line.mWidthPixels, Line.mGridSpacing, Line.mDepthMode);
    }
    if (!mLineRenderer.IsEmpty()) {
        const D3D11_VIEWPORT& Viewport{View.mTarget->GetViewport()};
        mLineRenderer.Render(mDeviceContext.Get(), FLineViewData{View.mCamera.mViewProjection, FVector2D{Viewport.Width, Viewport.Height}, Probe.mGridFade});
    }
}

void FRenderer::DrawOrientationAxis(const FRenderView& View) {
    constexpr float Margin{5.0f};
    const D3D11_VIEWPORT& Viewport{View.mTarget->GetViewport()};
    const float AvailableSize{std::min(Viewport.Width, Viewport.Height) - Margin * 2.0f};
    if (AvailableSize <= 0.0f) {
        return;
    }
    const float RequestedSize{View.mOrientationAxisSize > 0.0f ? View.mOrientationAxisSize : std::min(std::min(Viewport.Width, Viewport.Height) * 0.15f, 160.0f)};
    const float AxisSize{std::min(RequestedSize, AvailableSize)};
    const D3D11_VIEWPORT AxisViewport{Viewport.TopLeftX + Margin, Viewport.TopLeftY + Margin, AxisSize, AxisSize, Viewport.MinDepth, Viewport.MaxDepth};
    mDeviceContext->RSSetViewports(1, &AxisViewport);
    FMatrix AxisView{View.mCamera.mView};
    AxisView.Translation(FVector3{0.0f, 0.0f, 3.0f});
    const FMatrix Projection{FMatrix::CreateOrthographic(2.5f, 2.5f, 0.1f, 10.0f)};
    mLineRenderer.Clear();
    mLineRenderer.AddRay(FVector3{}, FVector3{1.0f, 0.0f, 0.0f}, 1.0f, FVector4{1.0f, 0.0f, 0.0f, 1.0f}, 3.0f);
    mLineRenderer.AddRay(FVector3{}, FVector3{0.0f, 1.0f, 0.0f}, 1.0f, FVector4{0.0f, 1.0f, 0.0f, 1.0f}, 3.0f);
    mLineRenderer.AddRay(FVector3{}, FVector3{0.0f, 0.0f, 1.0f}, 1.0f, FVector4{0.0f, 0.0f, 1.0f, 1.0f}, 3.0f);
    mLineRenderer.Render(mDeviceContext.Get(), FLineViewData{AxisView * Projection, FVector2D{AxisSize, AxisSize}});
}

void FRenderer::ReSize(Uint32 Width, Uint32 Height) {
    if (!mSwapChain || Width == 0 || Height == 0) {
        return;
    }

    mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    mBackBufferSurface->Resize(mDevice.Get(), Width, Height);
    mBackBufferWidth = Width;
    mBackBufferHeight = Height;
}

void FRenderer::Terminate() {
    mDeviceContext->ClearState();
    mLineRenderer.Reset();
    mMeshRenderer.Reset();
    mLightContextArray.Reset();

    if (mBackBufferSurface != nullptr) {
        mBackBufferSurface->Reset();
    }
    mBackBufferSurface.reset();
    mSwapChain.Reset();
}

void FRenderer::ReportLiveObjects() const {
#ifdef _DEBUG
    mDebugInterface->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
#endif
}

void FRenderer::CreateDeviceAndSwapChain(HWND WindowHandle) {
    D3D_FEATURE_LEVEL FeatureLevels[]{D3D_FEATURE_LEVEL_11_0};

    DXGI_SWAP_CHAIN_DESC SwapChainDesc{};
    SwapChainDesc.BufferDesc.Width = mBackBufferWidth;
    SwapChainDesc.BufferDesc.Height = mBackBufferHeight;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.OutputWindow = WindowHandle;
    SwapChainDesc.Windowed = TRUE;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

#ifdef _DEBUG
    ErrorHandler::ReportHRESULT(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc, &mSwapChain, &mDevice, nullptr, &mDeviceContext), "[ FRenderer ]", "Failed to create Direct3D device and swap chain.", ErrorHandler::EErrorLevel::Critical);
#else
    ErrorHandler::ReportHRESULT(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc, &mSwapChain, &mDevice, nullptr, &mDeviceContext), "[ FRenderer ]", "Failed to create Direct3D device and swap chain.", ErrorHandler::EErrorLevel::Critical);
#endif
    mSwapChain->GetDesc(&SwapChainDesc);
}

bool FRenderer::CreateSamplerStates() {
    bool Succeeded{true};

    auto CreateSampler{[this](std::size_t Slot, const D3D11_SAMPLER_DESC& Description, const char* Name) {
        const HRESULT Result{mDevice->CreateSamplerState(&Description, mSamplerStates[Slot].ReleaseAndGetAddressOf())};
        ErrorHandler::ReportHRESULT(Result, "[FRenderer]", std::string("Failed to create ") + Name + " sampler.", ErrorHandler::EErrorLevel::Critical);
        return SUCCEEDED(Result);
    }};

    auto MakeDescription{[](D3D11_FILTER Filter, D3D11_TEXTURE_ADDRESS_MODE AddressMode) {
        D3D11_SAMPLER_DESC Description{};
        Description.Filter = Filter;
        Description.AddressU = AddressMode;
        Description.AddressV = AddressMode;
        Description.AddressW = AddressMode;
        Description.MipLODBias = 0.0f;
        Description.MaxAnisotropy = Filter == D3D11_FILTER_ANISOTROPIC ? 8 : 1;
        Description.ComparisonFunc = D3D11_COMPARISON_NEVER;
        Description.MinLOD = 0.0f;
        Description.MaxLOD = D3D11_FLOAT32_MAX;
        return Description;
    }};

    Succeeded = CreateSampler(0, MakeDescription(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP), "LinearWrap") && Succeeded;
    Succeeded = CreateSampler(1, MakeDescription(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP), "LinearClamp") && Succeeded;
    Succeeded = CreateSampler(2, MakeDescription(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP), "PointClamp") && Succeeded;
    Succeeded = CreateSampler(3, MakeDescription(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_WRAP), "PointWrap") && Succeeded;
    Succeeded = CreateSampler(4, MakeDescription(D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP), "AnisotropicWrap") && Succeeded;

    D3D11_SAMPLER_DESC ShadowDescription{MakeDescription(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER)};
    ShadowDescription.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    ShadowDescription.BorderColor[0] = 1.0f;
    ShadowDescription.BorderColor[1] = 1.0f;
    ShadowDescription.BorderColor[2] = 1.0f;
    ShadowDescription.BorderColor[3] = 1.0f;
    Succeeded = CreateSampler(5, ShadowDescription, "ShadowCompare") && Succeeded;
    return Succeeded;
}
