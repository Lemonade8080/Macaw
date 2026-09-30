#pragma once

#include "Asset/Pipeline/FShader.h"
#include "Buffer/FGraphicsBuffer.h"
#include "FRenderQueue.h"

class FGpuOcclusionCulling {
private:
    struct FBounds {
        FVector4 mCenter{};
        FVector4 mExtents{};
    };

    struct FBatch {
        Uint32 mFirstRecord{};
        Uint32 mRecordCount{};
        Uint32 mIndexCount{};
        Uint32 mFirstIndex{};
    };

    struct FConstants {
        FMatrix mViewProjection{};
        Uint32 mSourceWidth{};
        Uint32 mSourceHeight{};
        Uint32 mDestinationWidth{};
        Uint32 mDestinationHeight{};
        Uint32 mObjectCount{};
        Uint32 mRecordCount{};
        Uint32 mBatchCount{};
        Uint32 mMipCount{};
        Uint32 mDispatchWidth{};
        float mDepthBias{0.0001f};
        Uint32 mCullingPass{};
        Uint32 mPadding{};
    };

    struct FBuffer {
        FGraphicsBuffer mBuffer{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mResourceView{};
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> mUnorderedView{};
        Uint32 mCapacity{};
    };

    struct FViewResources {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mDepthResource{};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> mHierarchy{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mHierarchyResource{};
        TArray<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mMipResources{};
        TArray<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> mMipOutputs{};
        FBuffer mBounds{};
        FBuffer mBatches{};
        FBuffer mInputRecords{};
        FBuffer mVisibility{};
        FBuffer mOutputRecords{};
        FBuffer mArguments{};
        D3D11_VIEWPORT mViewport{};
        Uint32 mHierarchyWidth{};
        Uint32 mHierarchyHeight{};
        Uint64 mLastUsedFrame{};
        const FRenderScene* mScene{nullptr};
        Uint64 mSceneId{};
        Uint64 mBoundsRevision{};
        Uint64 mTemplateRevision{};
        FObjectHandle mSelectedActorHandle{};
        ERenderMode mRenderMode{ERenderMode::Lit};
        bool mUseLOD{};
        bool mRenderSky{};
        FMatrix mViewProjection{};
        FMatrix mPreviousViewProjection{};
        Uint64 mHistoryFrame{};
        bool mHistoryReady{};
        TArray<FMeshDrawRecord> mRecordSnapshot{};
        TArray<FBatch> mBatchSnapshot{};
    };

    static_assert(sizeof(FBounds) == 32);
    static_assert(sizeof(FBatch) == 16);
    static_assert(sizeof(FConstants) == 112);

public:
    bool Initialize(ID3D11Device* Device);
    void Reset();
    void BeginFrame(Uint64 FrameSerial);

    bool Prepare(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderView& View, const FRenderScene& Scene, const FRenderQueue& Queue);
    bool HasHistory() const;
    bool DispatchPrevious(ID3D11DeviceContext* Context);
    bool DispatchCurrent(ID3D11DeviceContext* Context);
    bool CaptureDepth(ID3D11DeviceContext* Context);

    void BindDrawRecords(ID3D11DeviceContext* Context) const;
    ID3D11Buffer* GetArguments() const;
    bool IsRetest() const;

private:
    bool CreateHierarchyResources(ID3D11Device* Device, FViewResources& Resources, Uint32 Width, Uint32 Height);
    bool EnsureBuffer(ID3D11Device* Device, FBuffer& Buffer, Uint32 Count, Uint32 Stride, bool Writable, bool Indirect = false);
    bool BuildHierarchy(ID3D11DeviceContext* Context);
    bool DispatchVisibility(ID3D11DeviceContext* Context);
    bool UploadConstants(ID3D11DeviceContext* Context);
    bool DispatchGroups(ID3D11DeviceContext* Context, Uint32 GroupCount);
    void UnbindCompute(ID3D11DeviceContext* Context) const;

private:
    FShader mCopyShader{};
    FShader mReductionShader{};
    FShader mCullShader{};
    FShader mArgumentsShader{};
    FGraphicsBuffer mConstantBuffer{};
    FConstants mConstants{};
    TMap<IRenderSurface*, FViewResources> mViews{};
    FViewResources* mCurrentResources{nullptr};
    TArray<FBounds> mBounds{};
    TArray<FBatch> mBatches{};
    Uint64 mFrameSerial{};
    bool mDrawReady{};
    bool mPreviousReady{};
};
