#include "pch.h"
#include "FFrameResource.h"
#include "Core/Base/ErrorHandler.h"

#include <limits>

bool FFrameResource::Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context) {
    Reset();
    if (Device == nullptr || Context == nullptr) {
        return false;
    }
    D3D11_FEATURE_DATA_D3D11_OPTIONS Options{};
    if (FAILED(Device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &Options, sizeof(Options))) || !Options.MapNoOverwriteOnDynamicConstantBuffer || !Options.MapNoOverwriteOnDynamicBufferSRV) {
        ErrorHandler::Report("[ FFrameResource ]", "WRITE_NO_OVERWRITE support for dynamic constant buffers and buffer SRVs is required.", ErrorHandler::EErrorLevel::Error);
        return false;
    }
    if (!InitializeConstantBuffer(Device, mFrameBuffer, sizeof(FFrameConstants))) {
        Reset();
        return false;
    }
    return true;
}

void FFrameResource::Reset() {
    mFrameBuffer.Reset();
    mModelIndexBuffer.Reset();
    mViews.clear();
    mUsedViewCount = 0;
    mModelContexts.clear();
    mFrameConstants = {};
    mFrameReady = false;
    mViewReady = false;
    mHasCameraWorld = false;
}

bool FFrameResource::BeginFrame(ID3D11DeviceContext* Context, float AnimationTime) {
    if (mFrameReady) {
        return false;
    }
    mUsedViewCount = 0;
    mViewReady = false;
    mHasCameraWorld = false;
    mFrameConstants.mAnimationTime = AnimationTime;
    mFrameConstants.mAnimationFrame = static_cast<Uint32>(AnimationTime / 0.1f) % 250;
    mFrameReady = mFrameBuffer.WriteNoOverwrite(Context, &mFrameConstants, sizeof(mFrameConstants), 0);
    return mFrameReady;
}

void FFrameResource::EndFrame() {
    mFrameReady = false;
    mViewReady = false;
    mHasCameraWorld = false;
}

bool FFrameResource::PrepareView(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderView& View, const FSceneRenderData& Scene, const FRenderQueue& Queue) {
    mViewReady = false;
    mHasCameraWorld = false;
    if (!mFrameReady || Device == nullptr || Context == nullptr || View.mTarget == nullptr || !View.mTarget->IsValid()) {
        return false;
    }
    ID3D11ShaderResourceView* NullResource{nullptr};
    Context->PSSetShaderResources(2, 1, &NullResource);
    if (mUsedViewCount == mViews.size()) {
        FViewBuffers Buffers{};
        if (!InitializeConstantBuffer(Device, Buffers.mViewConstants, sizeof(FViewConstants)) || !InitializeConstantBuffer(Device, Buffers.mOrientationAxisConstants, sizeof(FViewConstants)) || !Buffers.mLights.Initialize(Device, Context, 16) || !Buffers.mModels.Initialize(Device, Context, 128)) {
            return false;
        }
        mViews.push_back(std::move(Buffers));
    }
    FViewBuffers& Buffers{mViews[mUsedViewCount++]};
    Buffers.mOrientationAxisReady = false;
    for (FStreamBuffer& Stream : Buffers.mStreams) {
        Stream.mUploaded = false;
    }
    if (!Buffers.mLights.UploadNoOverwrite(Device, Context, Scene.mLightProbes) || !UploadModels(Device, Context, Queue)) {
        return false;
    }
    mViewReady = UploadViewConstants(Context, Buffers.mViewConstants, View.mCamera, View.mTarget->GetViewport(), View.mGridFade, mHasCameraWorld);
    return mViewReady;
}

bool FFrameResource::PrepareOrientationAxis(ID3D11DeviceContext* Context, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport) {
    if (!mFrameReady || !mViewReady || mUsedViewCount == 0) {
        return false;
    }
    FViewBuffers& Buffers{mViews[mUsedViewCount - 1]};
    if (Buffers.mOrientationAxisReady) {
        return false;
    }
    bool HasCameraWorld{};
    Buffers.mOrientationAxisReady = UploadViewConstants(Context, Buffers.mOrientationAxisConstants, Camera, Viewport, FVector4{}, HasCameraWorld);
    return Buffers.mOrientationAxisReady;
}

bool FFrameResource::BindCommon(ID3D11DeviceContext* Context, bool OrientationAxis) const {
    if (Context == nullptr || !mFrameReady || !mViewReady || mUsedViewCount == 0) {
        return false;
    }
    const FViewBuffers& Buffers{mViews[mUsedViewCount - 1]};
    if (OrientationAxis && !Buffers.mOrientationAxisReady) {
        return false;
    }
    BindConstantBuffer(Context, 0, mFrameBuffer);
    BindConstantBuffer(Context, 1, OrientationAxis ? Buffers.mOrientationAxisConstants : Buffers.mViewConstants);
    Context->PSSetShaderResources(2, 1, Buffers.mLights.GetSRV());
    return true;
}

bool FFrameResource::BindModels(ID3D11DeviceContext* Context) const {
    if (!BindCommon(Context) || mUsedViewCount == 0 || mViews[mUsedViewCount - 1].mModels.IsEmpty() || !mModelIndexBuffer.IsValid()) {
        return false;
    }
    Context->VSSetShaderResources(0, 1, mViews[mUsedViewCount - 1].mModels.GetSRV());
    Context->PSSetShaderResources(0, 1, mViews[mUsedViewCount - 1].mModels.GetSRV());
    ID3D11Buffer* ModelIndexBuffer{mModelIndexBuffer.GetBuffer()};
    const UINT Stride{sizeof(Uint32)};
    const UINT Offset{};
    Context->IASetVertexBuffers(4, 1, &ModelIndexBuffer, &Stride, &Offset);
    return true;
}

bool FFrameResource::BindMeshDraw(ID3D11DeviceContext* Context, Uint32 ModelIndex) const {
    if (Context == nullptr || !mViewReady || mUsedViewCount == 0 || ModelIndex >= mViews[mUsedViewCount - 1].mModels.GetCount() || !mModelIndexBuffer.IsValid()) {
        return false;
    }
    ID3D11Buffer* ModelIndexBuffer{mModelIndexBuffer.GetBuffer()};
    const UINT Stride{sizeof(Uint32)};
    const UINT Offset{ModelIndex * Stride};
    Context->IASetVertexBuffers(4, 1, &ModelIndexBuffer, &Stride, &Offset);
    return true;
}

bool FFrameResource::UploadStream(ID3D11Device* Device, ID3D11DeviceContext* Context, EFrameStream Stream, const void* Data, Uint32 Count, Uint32 Stride, Uint32 BindFlags) {
    const std::size_t Index{static_cast<std::size_t>(Stream)};
    if (!mFrameReady || !mViewReady || mUsedViewCount == 0 || Index >= static_cast<std::size_t>(EFrameStream::Count) || Device == nullptr || Context == nullptr || Data == nullptr || Count == 0 || Stride == 0 || Count > UINT32_MAX / Stride) {
        return false;
    }
    FStreamBuffer& Destination{mViews[mUsedViewCount - 1].mStreams[Index]};
    if (Destination.mUploaded) {
        return false;
    }
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
    if (Destination.mBuffer.GetStride() != (BindFlags == D3D11_BIND_SHADER_RESOURCE ? Stride : 0) || Destination.mBuffer.GetBindFlags() != BindFlags || !Destination.mBuffer.WriteNoOverwrite(Context, Data, Count * Stride, 0)) {
        return false;
    }
    Destination.mUploaded = true;
    return true;
}

ID3D11Buffer* FFrameResource::GetStreamBuffer(EFrameStream Stream) const {
    const std::size_t Index{static_cast<std::size_t>(Stream)};
    if (!mFrameReady || !mViewReady || mUsedViewCount == 0 || Index >= static_cast<std::size_t>(EFrameStream::Count)) {
        return nullptr;
    }
    const FStreamBuffer& Buffer{mViews[mUsedViewCount - 1].mStreams[Index]};
    return Buffer.mUploaded ? Buffer.mBuffer.GetBuffer() : nullptr;
}

ID3D11ShaderResourceView* FFrameResource::GetStreamResourceView(EFrameStream Stream) const {
    const std::size_t Index{static_cast<std::size_t>(Stream)};
    if (!mFrameReady || !mViewReady || mUsedViewCount == 0 || Index >= static_cast<std::size_t>(EFrameStream::Count)) {
        return nullptr;
    }
    const FStreamBuffer& Buffer{mViews[mUsedViewCount - 1].mStreams[Index]};
    return Buffer.mUploaded ? Buffer.mResourceView.Get() : nullptr;
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

bool FFrameResource::UploadViewConstants(ID3D11DeviceContext* Context, FGraphicsBuffer& Buffer, const CameraProbe& Camera, const D3D11_VIEWPORT& Viewport, const FVector4& GridFade, bool& HasCameraWorld) {
    if (Context == nullptr || Viewport.Width <= 0.0f || Viewport.Height <= 0.0f) {
        return false;
    }
    FViewConstants Constants{};
    Constants.mView = Camera.mView;
    Constants.mProjection = Camera.mProjection;
    Constants.mViewProjection = Camera.mViewProjection;
    HasCameraWorld = Camera.mView.TryInverse(Constants.mCameraWorld);
    Constants.mViewport = FVector4{Viewport.Width, Viewport.Height, 1.0f / Viewport.Width, 1.0f / Viewport.Height};
    Constants.mGridFade = GridFade;
    Constants.mLightCount = mViews[mUsedViewCount - 1].mLights.GetCount();
    return Buffer.WriteNoOverwrite(Context, &Constants, sizeof(Constants), 0);
}

bool FFrameResource::UploadModels(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderQueue& Queue) {
    mModelContexts.clear();
    const TArray<FMeshDrawItem>& SceneItems{Queue.GetItems(ERenderPass::SceneGeometry)};
    const TArray<FMeshDrawItem>& GizmoItems{Queue.GetItems(ERenderPass::Gizmo)};
    constexpr std::size_t MaxModelCount{UINT32_MAX / sizeof(FModelContext)};
    if (SceneItems.size() > MaxModelCount || GizmoItems.size() > MaxModelCount - SceneItems.size()) {
        return false;
    }
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
    FViewBuffers& Buffers{mViews[mUsedViewCount - 1]};
    return Buffers.mModels.UploadNoOverwrite(Device, Context, mModelContexts) && (Buffers.mModels.IsEmpty() || EnsureModelIndices(Device));
}

bool FFrameResource::EnsureModelIndices(ID3D11Device* Device) {
    const Uint32 Capacity{mViews[mUsedViewCount - 1].mModels.GetCapacity()};
    if (mModelIndexBuffer.GetByteSize() / sizeof(Uint32) >= Capacity) {
        return true;
    }
    TArray<Uint32> ModelIndices{};
    ModelIndices.resize(Capacity);
    for (Uint32 Index{}; Index < Capacity; ++Index) {
        ModelIndices[Index] = Index;
    }
    FGraphicsBufferDescription Description{};
    Description.mByteSize = Capacity * sizeof(Uint32);
    Description.mUsage = D3D11_USAGE_IMMUTABLE;
    Description.mBindFlags = D3D11_BIND_VERTEX_BUFFER;
    return mModelIndexBuffer.Initialize(Device, Description, ModelIndices.data());
}

void FFrameResource::BindConstantBuffer(ID3D11DeviceContext* Context, Uint32 Slot, const FGraphicsBuffer& Buffer) const {
    ID3D11Buffer* ConstantBuffer{Buffer.GetBuffer()};
    Context->VSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->HSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->DSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->GSSetConstantBuffers(Slot, 1, &ConstantBuffer);
    Context->PSSetConstantBuffers(Slot, 1, &ConstantBuffer);
}
