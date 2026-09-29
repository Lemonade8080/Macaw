#pragma once

#include "FRenderQueue.h"
#include "Buffer/TGraphicsArray.h"

#include <array>

enum class EFrameStream : Uint8 {
    Text,
    Billboard,
    LineDepth,
    LineOverlay,
    BatchLineDepth,
    BatchLineOverlay,
    Count
};

class FFrameResource {
private:
    struct FFrameConstants {
        Uint32 mAnimationFrame{};
        float mAnimationTime{};
        FVector2 mPadding{};
    };

    struct FViewConstants {
        FMatrix mView{};
        FMatrix mProjection{};
        FMatrix mViewProjection{};
        FMatrix mCameraWorld{};
        FVector4 mViewport{};
        FVector4 mGridFade{};
        Uint32 mLightCount{};
        FVector3 mPadding{};
    };

    struct FTextConstants {
        FMatrix mWorld{};
        FVector4 mColor{};
        FVector3 mScreenBoundsExtent{};
        float mScreenUpPadding{};
    };

    struct FModelContext {
        FMatrix mWorld{};
        Uint32 mMaterialIndex{UINT32_MAX};
        Uint32 mFlags{};
    };

    struct FStreamBuffer {
        FGraphicsBuffer mBuffer{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mResourceView{};
        Uint32 mCapacity{};
    };

    static_assert(sizeof(FFrameConstants) == 16);
    static_assert(sizeof(FViewConstants) == 304);
    static_assert(sizeof(FTextConstants) == 96);
    static_assert(sizeof(FModelContext) == 72);

public:
    bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context);
    void Reset();

    bool BeginFrame(ID3D11DeviceContext* Context, float AnimationTime);
    void EndFrame();

    bool PrepareView(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderView& View, const FSceneRenderData& Scene, const FRenderQueue& Queue);
    bool UpdateView(ID3D11DeviceContext* Context, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport, const FVector4& GridFade);
    bool BindCommon(ID3D11DeviceContext* Context) const;
    bool BindModels(ID3D11DeviceContext* Context) const;
    bool BindMeshDraw(ID3D11DeviceContext* Context, Uint32 ModelIndex) const;
    bool BindTextDraw(ID3D11DeviceContext* Context, const FTextProbe& Probe);

    bool UploadStream(ID3D11Device* Device, ID3D11DeviceContext* Context, EFrameStream Stream, const void* Data, Uint32 Count, Uint32 Stride, Uint32 BindFlags);
    ID3D11Buffer* GetStreamBuffer(EFrameStream Stream) const;
    ID3D11ShaderResourceView* GetStreamResourceView(EFrameStream Stream) const;
    bool HasCameraWorld() const;

private:
    bool InitializeConstantBuffer(ID3D11Device* Device, FGraphicsBuffer& Buffer, Uint32 ByteSize);
    bool UploadModels(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderQueue& Queue);
    bool EnsureModelIndices(ID3D11Device* Device);
    void BindConstantBuffer(ID3D11DeviceContext* Context, Uint32 Slot, const FGraphicsBuffer& Buffer) const;

private:
    FGraphicsBuffer mFrameBuffer{};
    FGraphicsBuffer mViewBuffer{};
    FGraphicsBuffer mModelIndexBuffer{};
    FGraphicsBuffer mTextBuffer{};
    TGraphicsArray<FLightProbe, true, true> mLights{};
    TGraphicsArray<FModelContext, true, true> mModels{};
    TArray<FModelContext> mModelContexts{};
    std::array<FStreamBuffer, static_cast<std::size_t>(EFrameStream::Count)> mStreams{};
    FFrameConstants mFrameConstants{};
    bool mFrameReady{};
    bool mViewReady{};
    bool mHasCameraWorld{};
};
