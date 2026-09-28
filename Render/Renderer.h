#pragma once

#include <d3d11_4.h>
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
#include "FFrameResource.h"

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
    void RenderView(const FRenderView& View, const FSceneRenderData& Scene);
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

    void ExecutePass(ERenderPass Pass, const FRenderContext& Context, const FRenderView& View, const FSceneRenderData& Scene);
    void DrawSceneGuides(const FRenderView& View);
    void DrawOrientationAxis(const FRenderView& View);

private:
    static constexpr Uint32 mFrameResourceCount{3};
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D11Debug> mDebugInterface{};
#endif
    Microsoft::WRL::ComPtr<ID3D11Device> mDevice{};
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContext{};
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> mFenceContext{};
    Microsoft::WRL::ComPtr<ID3D11Fence> mFrameFence{};
    HANDLE mFrameFenceEvent{nullptr};
    Uint64 mNextFenceValue{1};
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain{};
    std::unique_ptr<IRenderSurface> mBackBufferSurface{};
    // s0: LinearWrap, s1: LinearClamp, s2: PointClamp, s3: PointWrap, s4: AnisotropicWrap, s5: ShadowCompare.
    std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, 6> mSamplerStates{};
    IRenderAssetRegistry* mAssetRegistry{nullptr};

    std::array<FFrameResource, mFrameResourceCount> mFrameResources{};
    FFrameResource* mCurrentFrameResource{nullptr};
    Uint32 mNextFrameResourceIndex{};
    float mAnimationTime{};
    FRenderQueue mRenderQueue{};
    FMeshRenderer mMeshRenderer{};
    FTextRenderer mTextRenderer{};
    FBillboardRenderer mBillboardRenderer{};
    FLineRenderer mLineRenderer{};

    const float mUiClearColor[4]{0.2f, 0.2f, 0.7f, 1.0f};
    Uint32 mBackBufferWidth{};
    Uint32 mBackBufferHeight{};
};
