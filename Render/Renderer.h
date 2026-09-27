#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <memory>

#include "Asset/IRenderAssetRegistry.h"
#include "FRenderView.h"
#include "FRenderQueue.h"
#include "FMeshRenderer.h"
#include "FTextRenderer.h"
#include "FBillboardRenderer.h"
#include "FLineRenderer.h"
#include "FSceneRenderSurface.h"

class FRenderer {
public:
    FRenderer() = default;
    ~FRenderer();

    FRenderer(const FRenderer&) = delete;
    FRenderer& operator=(const FRenderer&) = delete;
    FRenderer(FRenderer&&) = delete;
    FRenderer& operator=(FRenderer&&) = delete;

public:
    void Create(HWND WindowHandle, UINT Width, UINT Height);
    bool Initialize();

    void BeginFrame(float DeltaTime);
    void RenderView(const FRenderView& View, const FRenderProbe& Probe);
    void BeginUiRender();
    void EndFrame();

    ID3D11Device* GetDevice() const;
    ID3D11DeviceContext* GetDeviceContext() const;
    void BindAssetRegistry(IRenderAssetRegistry* InAssetRegistry);

    void ReSize(Uint32 Width, Uint32 Height);
    void Terminate();
    void ReportLiveObjects() const;

private:
    void CreateDeviceAndSwapChain(HWND WindowHandle);
    bool CreateSamplerStates();
    void BindSamplerStates();
    bool UploadLightContext(const FRenderProbe& Probe);

    void ExecutePass(ERenderPass Pass, const FRenderContext& Context, const FRenderView& View, const FRenderProbe& Probe);
    void DrawSceneGuides(const FRenderView& View, const FRenderProbe& Probe);
    void DrawOrientationAxis(const FRenderView& View);

private:
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D11Debug> mDebugInterface{};
#endif
    Microsoft::WRL::ComPtr<ID3D11Device> mDevice{};
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContext{};
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain{};
    std::unique_ptr<IRenderSurface> mBackBufferSurface{};
    // s0: LinearWrap, s1: LinearClamp, s2: PointClamp, s3: PointWrap, s4: AnisotropicWrap, s5: ShadowCompare.
    std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, 6> mSamplerStates{};
    IRenderAssetRegistry* mAssetRegistry{nullptr};

    TGraphicsArray<FLightProbe, true, true> mLightContextArray{};
    FRenderQueue mRenderQueue{};
    FMeshRenderer mMeshRenderer{};
    FTextRenderer mTextRenderer{};
    FBillboardRenderer mBillboardRenderer{};
    FLineRenderer mLineRenderer{};

    const float mUiClearColor[4]{0.2f, 0.2f, 0.7f, 1.0f};
    Uint32 mFrameLightCount{};
    Uint32 mBackBufferWidth{};
    Uint32 mBackBufferHeight{};
    //VAT current time 계산용(임시)
    float mAnimationTime{};
    Uint32 mAnimationFrame{};
};
