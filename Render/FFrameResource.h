#pragma once

#include "FRenderQueue.h"
#include "Buffer/TGraphicsArray.h"

#include <array>

enum class EFrameStream : Uint8 {
    Text,
    TextContext,
    Billboard,
    LineDepth,
    LineOverlay,
    BatchLineDepth,
    BatchLineOverlay,
    OrientationAxisLineDepth,
    OrientationAxisLineOverlay,
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

    struct FModelContext {
        FMatrix mWorld{};
        Uint32 mMaterialIndex{UINT32_MAX};
        Uint32 mFlags{};
    };

    struct FStreamBuffer {
        FGraphicsBuffer mBuffer{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mResourceView{};
        Uint32 mCapacity{};
        bool mUploaded{};
    };

    struct FViewBuffers {
        FGraphicsBuffer mViewConstants{};
        FGraphicsBuffer mOrientationAxisConstants{};
        TGraphicsArray<FLightProbe, true, true> mLights{};
        TGraphicsArray<FModelContext, true, true> mModels{};
        std::array<FStreamBuffer, static_cast<std::size_t>(EFrameStream::Count)> mStreams{};
        bool mOrientationAxisReady{};
    };

    static_assert(sizeof(FFrameConstants) == 16);
    static_assert(sizeof(FViewConstants) == 304);
    static_assert(sizeof(FModelContext) == 72);

public:
    bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context);
    void Reset();

    bool BeginFrame(ID3D11DeviceContext* Context, float AnimationTime);
    void EndFrame();

    bool PrepareView(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderView& View, const FSceneRenderData& Scene, const FRenderQueue& Queue);
    bool PrepareOrientationAxis(ID3D11DeviceContext* Context, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport);
    bool BindCommon(ID3D11DeviceContext* Context, bool OrientationAxis = false) const;
    bool BindModels(ID3D11DeviceContext* Context) const;
    bool BindMeshDraw(ID3D11DeviceContext* Context, Uint32 ModelIndex) const;

    bool UploadStream(ID3D11Device* Device, ID3D11DeviceContext* Context, EFrameStream Stream, const void* Data, Uint32 Count, Uint32 Stride, Uint32 BindFlags);
    ID3D11Buffer* GetStreamBuffer(EFrameStream Stream) const;
    ID3D11ShaderResourceView* GetStreamResourceView(EFrameStream Stream) const;
    bool HasCameraWorld() const;

private:
    bool InitializeConstantBuffer(ID3D11Device* Device, FGraphicsBuffer& Buffer, Uint32 ByteSize);
    bool UploadViewConstants(ID3D11DeviceContext* Context, FGraphicsBuffer& Buffer, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport, const FVector4& GridFade, bool& HasCameraWorld);
    bool UploadModels(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderQueue& Queue);
    bool EnsureModelIndices(ID3D11Device* Device);
    void BindConstantBuffer(ID3D11DeviceContext* Context, Uint32 Slot, const FGraphicsBuffer& Buffer) const;

private:
    FGraphicsBuffer mFrameBuffer{};
    FGraphicsBuffer mModelIndexBuffer{};
    TArray<FViewBuffers> mViews{};
    std::size_t mUsedViewCount{};
    TArray<FModelContext> mModelContexts{};
    FFrameConstants mFrameConstants{};
    bool mFrameReady{};
    bool mViewReady{};
    bool mHasCameraWorld{};
};
