#include "pch.h"
#include "FFrameResource.h"

#include <limits>

bool FFrameResource::Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context) {
    Reset();
    if (Device == nullptr || Context == nullptr) {
        return false;
    }
    if (!InitializeConstantBuffer(Device, mFrameBuffer, sizeof(FFrameConstants)) || !InitializeConstantBuffer(Device, mViewBuffer, sizeof(FViewConstants)) || !InitializeConstantBuffer(Device, mDrawBuffer, sizeof(FDrawConstants)) || !InitializeConstantBuffer(Device, mTextBuffer, sizeof(FTextConstants)) || !mLights.Initialize(Device, Context, 16) || !mModels.Initialize(Device, Context, 128)) {
        Reset();
        return false;
    }
    return true;
}

void FFrameResource::Reset() {
    mFrameBuffer.Reset();
    mViewBuffer.Reset();
    mDrawBuffer.Reset();
    mTextBuffer.Reset();
    mLights.Reset();
    mModels.Reset();
    mModelContexts.clear();
    for (FStreamBuffer& Stream : mStreams) {
        Stream.mBuffer.Reset();
        Stream.mResourceView.Reset();
        Stream.mCapacity = 0;
    }
    mFrameConstants = {};
    mFrameReady = false;
    mViewReady = false;
    mHasCameraWorld = false;
    mCompletionValue = 0;
}

bool FFrameResource::BeginFrame(ID3D11DeviceContext* Context, float AnimationTime) {
    mViewReady = false;
    mHasCameraWorld = false;
    mFrameConstants.mAnimationTime = AnimationTime;
    mFrameConstants.mAnimationFrame = static_cast<Uint32>(AnimationTime / 0.1f) % 250;
    mFrameReady = mFrameBuffer.WriteDiscard(Context, &mFrameConstants, sizeof(mFrameConstants));
    return mFrameReady;
}

void FFrameResource::SetCompletionValue(Uint64 CompletionValue) {
    mCompletionValue = CompletionValue;
    mFrameReady = false;
    mViewReady = false;
    mHasCameraWorld = false;
}

Uint64 FFrameResource::GetCompletionValue() const {
    return mCompletionValue;
}

bool FFrameResource::PrepareView(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderView& View, const FSceneRenderData& Scene, const FRenderQueue& Queue) {
    mViewReady = false;
    mHasCameraWorld = false;
    if (!mFrameReady || Device == nullptr || Context == nullptr || View.mTarget == nullptr || !View.mTarget->IsValid()) {
        return false;
    }
    ID3D11ShaderResourceView* NullResource{nullptr};
    Context->PSSetShaderResources(2, 1, &NullResource);
    if (!mLights.UploadDiscard(Device, Context, Scene.mLightProbes) || !UploadModels(Device, Context, Queue)) {
        return false;
    }
    return UpdateView(Context, View.mCamera, View.mTarget->GetViewport(), View.mGridFade);
}

bool FFrameResource::UpdateView(ID3D11DeviceContext* Context, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport, const FVector4& GridFade) {
    mViewReady = false;
    mHasCameraWorld = false;
    if (!mFrameReady || Context == nullptr || Viewport.Width <= 0.0f || Viewport.Height <= 0.0f) {
        return false;
    }
    FViewConstants Constants{};
    Constants.mView = Camera.mView;
    Constants.mProjection = Camera.mProjection;
    Constants.mViewProjection = Camera.mViewProjection;
    mHasCameraWorld = Camera.mView.TryInverse(Constants.mCameraWorld);
    Constants.mViewport = FVector4{Viewport.Width, Viewport.Height, 1.0f / Viewport.Width, 1.0f / Viewport.Height};
    Constants.mGridFade = GridFade;
    Constants.mLightCount = mLights.GetCount();
    mViewReady = mViewBuffer.WriteDiscard(Context, &Constants, sizeof(Constants));
    return mViewReady;
}

bool FFrameResource::BindCommon(ID3D11DeviceContext* Context) const {
    if (Context == nullptr || !mFrameReady || !mViewReady) {
        return false;
    }
    BindConstantBuffer(Context, 0, mFrameBuffer);
    BindConstantBuffer(Context, 1, mViewBuffer);
    Context->PSSetShaderResources(2, 1, mLights.GetSRV());
    return true;
}

bool FFrameResource::BindModels(ID3D11DeviceContext* Context) const {
    if (!BindCommon(Context) || mModels.IsEmpty()) {
        return false;
    }
    Context->VSSetShaderResources(0, 1, mModels.GetSRV());
    Context->PSSetShaderResources(0, 1, mModels.GetSRV());
    BindConstantBuffer(Context, 2, mDrawBuffer);
    return true;
}

bool FFrameResource::BindMeshDraw(ID3D11DeviceContext* Context, Uint32 ModelIndex) {
    const FDrawConstants Constants{ModelIndex};
    return mViewReady && ModelIndex < mModels.GetCount() && mDrawBuffer.WriteDiscard(Context, &Constants, sizeof(Constants));
}

bool FFrameResource::BindTextDraw(ID3D11DeviceContext* Context, const FTextProbe& Probe) {
    const FTextConstants Constants{Probe.mWorld, Probe.mColor, Probe.mScreenBoundsExtent, Probe.mScreenUpPadding};
    if (!mViewReady || !mHasCameraWorld || !mTextBuffer.WriteDiscard(Context, &Constants, sizeof(Constants))) {
        return false;
    }
    ID3D11Buffer* Buffer{mTextBuffer.GetBuffer()};
    Context->GSSetConstantBuffers(2, 1, &Buffer);
    Context->PSSetConstantBuffers(2, 1, &Buffer);
    return true;
}

bool FFrameResource::UploadStream(ID3D11Device* Device, ID3D11DeviceContext* Context, EFrameStream Stream, const void* Data, Uint32 Count, Uint32 Stride, Uint32 BindFlags) {
    const std::size_t Index{static_cast<std::size_t>(Stream)};
    if (Index >= mStreams.size() || Device == nullptr || Context == nullptr || Data == nullptr || Count == 0 || Stride == 0 || Count > UINT32_MAX / Stride) {
        return false;
    }
    FStreamBuffer& Destination{mStreams[Index]};
    if (Destination.mCapacity < Count) {
        Uint32 Capacity{std::max(Destination.mCapacity, 1u)};
        while (Capacity < Count && Capacity <= UINT32_MAX / 2) {
            Capacity *= 2;
        }
        Capacity = std::max(Capacity, Count);
        if (Capacity > UINT32_MAX / Stride) {
            Capacity = Count;
        }
        FGraphicsBufferDescription Description{};
        Description.mByteSize = Capacity * Stride;
        Description.mStride = BindFlags == D3D11_BIND_SHADER_RESOURCE ? Stride : 0;
        Description.mUsage = D3D11_USAGE_DYNAMIC;
        Description.mBindFlags = BindFlags;
        Description.mCpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Description.mMiscFlags = BindFlags == D3D11_BIND_SHADER_RESOURCE ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : 0;
        FGraphicsBuffer Buffer{};
        if (!Buffer.Initialize(Device, Description)) {
            return false;
        }
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ResourceView{};
        if (BindFlags == D3D11_BIND_SHADER_RESOURCE) {
            D3D11_SHADER_RESOURCE_VIEW_DESC ViewDescription{};
            ViewDescription.Format = DXGI_FORMAT_UNKNOWN;
            ViewDescription.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            ViewDescription.Buffer.NumElements = Capacity;
            if (FAILED(Device->CreateShaderResourceView(Buffer.GetBuffer(), &ViewDescription, ResourceView.GetAddressOf()))) {
                return false;
            }
        }
        Destination.mBuffer = std::move(Buffer);
        Destination.mResourceView = std::move(ResourceView);
        Destination.mCapacity = Capacity;
    }
    return Destination.mBuffer.GetStride() == (BindFlags == D3D11_BIND_SHADER_RESOURCE ? Stride : 0) && Destination.mBuffer.GetBindFlags() == BindFlags && Destination.mBuffer.WriteDiscard(Context, Data, Count * Stride);
}

ID3D11Buffer* FFrameResource::GetStreamBuffer(EFrameStream Stream) const {
    const std::size_t Index{static_cast<std::size_t>(Stream)};
    return Index < mStreams.size() ? mStreams[Index].mBuffer.GetBuffer() : nullptr;
}

ID3D11ShaderResourceView* FFrameResource::GetStreamResourceView(EFrameStream Stream) const {
    const std::size_t Index{static_cast<std::size_t>(Stream)};
    return Index < mStreams.size() ? mStreams[Index].mResourceView.Get() : nullptr;
}

bool FFrameResource::HasCameraWorld() const {
    return mViewReady && mHasCameraWorld;
}

bool FFrameResource::InitializeConstantBuffer(ID3D11Device* Device, FGraphicsBuffer& Buffer, Uint32 ByteSize) {
    FGraphicsBufferDescription Description{};
    Description.mByteSize = ByteSize;
    Description.mUsage = D3D11_USAGE_DYNAMIC;
    Description.mBindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Description.mCpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return Buffer.Initialize(Device, Description);
}

bool FFrameResource::UploadModels(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderQueue& Queue) {
    mModelContexts.clear();
    const TArray<FMeshDrawItem>& SceneItems{Queue.GetItems(ERenderPass::SceneGeometry)};
    const TArray<FMeshDrawItem>& GizmoItems{Queue.GetItems(ERenderPass::Gizmo)};
    mModelContexts.reserve(SceneItems.size() + GizmoItems.size());
    for (const FMeshDrawItem& Item : SceneItems) {
        mModelContexts.push_back(FModelContext{Item.mProbe.mWorld, Item.mMaterialIndex, Item.mProbe.mFlags});
    }
    for (const FMeshDrawItem& Item : GizmoItems) {
        mModelContexts.push_back(FModelContext{Item.mProbe.mWorld, Item.mMaterialIndex, Item.mProbe.mFlags});
    }
    ID3D11ShaderResourceView* NullResource{nullptr};
    Context->VSSetShaderResources(0, 1, &NullResource);
    Context->PSSetShaderResources(0, 1, &NullResource);
    return mModels.UploadDiscard(Device, Context, mModelContexts);
}

void FFrameResource::BindConstantBuffer(ID3D11DeviceContext* Context, Uint32 Slot, const FGraphicsBuffer& Buffer) const {
    ID3D11Buffer* ConstantBuffer{Buffer.GetBuffer()};
    Context->VSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->HSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->DSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->GSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->PSSetConstantBuffers(Slot, 1, &ConstantBuffer);
}
