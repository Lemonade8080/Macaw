#include "pch.h"
#include "FGpuOcclusionCulling.h"
#include "RenderConfig.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

bool FGpuOcclusionCulling::Initialize(ID3D11Device* Device) {
    Reset();
    FShaderDescription Description{};
    Description.mSource = "./Content/Shader/OcclusionCulling.hlsl";
    Description.mProfile = "cs_5_0";
    Description.mStage = EShaderStage::Compute;
    Description.mEntryPoint = "CopyDepth";
    if (!mCopyShader.Initialize(Device, Description)) {
        return false;
    }
    Description.mEntryPoint = "ReduceDepth";
    if (!mReductionShader.Initialize(Device, Description)) {
        return false;
    }
    Description.mEntryPoint = "CullObjects";
    if (!mCullShader.Initialize(Device, Description)) {
        return false;
    }
#if ENABLE_INSTANCE
    Description.mEntryPoint = "BuildInstancedArguments";
#else
    Description.mEntryPoint = "BuildObjectArguments";
#endif
    if (!mArgumentsShader.Initialize(Device, Description)) {
        return false;
    }
    FGraphicsBufferDescription BufferDescription{};
    BufferDescription.mByteSize = sizeof(FConstants);
    BufferDescription.mUsage = D3D11_USAGE_DYNAMIC;
    BufferDescription.mBindFlags = D3D11_BIND_CONSTANT_BUFFER;
    BufferDescription.mCpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return mConstantBuffer.Initialize(Device, BufferDescription);
}

void FGpuOcclusionCulling::Reset() {
    mCurrentResources = nullptr;
    mDrawReady = false;
    mPreviousReady = false;
    mViews.clear();
    mBounds.clear();
    mBatches.clear();
    mCopyShader.Reset();
    mReductionShader.Reset();
    mCullShader.Reset();
    mArgumentsShader.Reset();
    mConstantBuffer.Reset();
    mConstants = {};
    mFrameSerial = 0;
}

void FGpuOcclusionCulling::BeginFrame(Uint64 FrameSerial) {
    mCurrentResources = nullptr;
    mDrawReady = false;
    mPreviousReady = false;
    mFrameSerial = FrameSerial;
    std::erase_if(mViews, [FrameSerial](const auto& Entry) {
        return FrameSerial - Entry.second.mLastUsedFrame > 120;
    });
}

bool FGpuOcclusionCulling::Prepare(ID3D11Device* Device, ID3D11DeviceContext* Context, const FRenderView& View, const FRenderScene& Scene, const FRenderQueue& Queue) {
    mCurrentResources = nullptr;
    mDrawReady = false;
    mPreviousReady = false;
    const TArray<FMeshDrawBatch>& Items{Queue.GetItems(ERenderPass::SceneGeometry)};
    if (Device == nullptr || Context == nullptr || View.mTarget == nullptr || View.mTarget->GetDepthShaderResourceView() == nullptr || !View.mSettings.mOcclusionCulling || View.mRenderMode == ERenderMode::Wireframe || Items.empty() || !mConstantBuffer.IsValid()) {
        return false;
    }
    const D3D11_VIEWPORT& Viewport{View.mTarget->GetViewport()};
    if (!std::isfinite(Viewport.Width) || !std::isfinite(Viewport.Height) || Viewport.Width < 1.0f || Viewport.Height < 1.0f || Viewport.Width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || Viewport.Height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || Viewport.TopLeftX != 0.0f || Viewport.TopLeftY != 0.0f || Viewport.MinDepth != 0.0f || Viewport.MaxDepth != 1.0f) {
        return false;
    }
    const Uint32 Width{static_cast<Uint32>(Viewport.Width)};
    const Uint32 Height{static_cast<Uint32>(Viewport.Height)};
    const TArray<FRenderSceneObject>& Objects{Scene.GetObjects()};
    const TArray<FRenderTemplateGroup>& Groups{Scene.GetTemplateGroups()};
    if (Objects.empty() || Objects.size() > UINT32_MAX || Items.size() > UINT32_MAX) {
        return false;
    }
    bool HasOccluders{};
    for (const FRenderTemplateGroup& Group : Groups) {
        if (Group.mReferenceCount == 0 || Group.mPipeline == nullptr) {
            continue;
        }
        if (!Group.mPipeline->CanReuseOcclusionDepth(View.mRenderMode)) {
            return false;
        }
        HasOccluders = HasOccluders || (!Group.mSky && Group.mPipeline->CanWriteOcclusionDepth(View.mRenderMode));
    }
    if (!HasOccluders) {
        return false;
    }
    const FMeshDrawBatch& Last{Items.back()};
    if (Last.mRecordCount > UINT32_MAX - Last.mFirstRecord) {
        return false;
    }
    const Uint32 RecordCount{Last.mFirstRecord + Last.mRecordCount};
    if (RecordCount < 2 || RecordCount > Queue.GetDrawRecords().size()) {
        return false;
    }
    FViewResources& Resources{mViews[View.mTarget]};
    Resources.mLastUsedFrame = mFrameSerial;
    if (!CreateHierarchyResources(Device, Resources, Width, Height)) {
        return false;
    }
    const bool SameScene{Resources.mScene == &Scene && Resources.mSceneId == Scene.GetId()};
    const bool SameSettings{Resources.mRenderMode == View.mRenderMode && Resources.mUseLOD == View.mUseLOD && Resources.mRenderSky == View.mSettings.mBRenderSky};
    Resources.mHistoryReady = Resources.mHistoryReady && Resources.mHistoryFrame + 1 == mFrameSerial && Resources.mDepthResource.Get() == View.mTarget->GetDepthShaderResourceView() && SameScene && SameSettings;
    Resources.mDepthResource = View.mTarget->GetDepthShaderResourceView();
    Resources.mViewProjection = View.mCamera.mViewProjection;
    const Uint32 ObjectCount{static_cast<Uint32>(Objects.size())};
    const Uint32 BatchCount{static_cast<Uint32>(Items.size())};
#if ENABLE_INSTANCE
    const Uint32 ArgumentCount{BatchCount};
#else
    const Uint32 ArgumentCount{RecordCount};
#endif
    const bool BoundsChanged{!SameScene || !SameSettings || Resources.mBoundsRevision != Scene.GetRevision() || Resources.mTemplateRevision != Scene.GetTemplateRevision() || Resources.mSelectedActorHandle != View.mSelectedActorHandle || ObjectCount > Resources.mBounds.mCapacity};
    const bool BatchesGrown{BatchCount > Resources.mBatches.mCapacity};
    const bool RecordsGrown{RecordCount > Resources.mInputRecords.mCapacity};
    if (!EnsureBuffer(Device, Resources.mBounds, ObjectCount, sizeof(FBounds), false) || !EnsureBuffer(Device, Resources.mBatches, BatchCount, sizeof(FBatch), false) || !EnsureBuffer(Device, Resources.mInputRecords, RecordCount, sizeof(FMeshDrawRecord), false) || !EnsureBuffer(Device, Resources.mVisibility, ObjectCount, sizeof(Uint32), true) || !EnsureBuffer(Device, Resources.mArguments, ArgumentCount, sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS), true, true)) {
        return false;
    }
#if ENABLE_INSTANCE
    if (!EnsureBuffer(Device, Resources.mOutputRecords, RecordCount, sizeof(FMeshDrawRecord), true)) {
        return false;
    }
#endif
    if (BoundsChanged) {
        mBounds.resize(ObjectCount);
        for (Uint32 Index{}; Index < ObjectCount; ++Index) {
            const FRenderSceneObject& Object{Objects[Index]};
            FBounds& Bounds{mBounds[Index]};
            Bounds = {};
            if (!Object.mActive || !Object.mCullable || Object.mTemplateGroupIndex >= Groups.size() || (Object.mFlags & static_cast<Uint32>(ERenderObjectFlags::Selected)) != 0 || (View.mSelectedActorHandle.IsValid() && Object.mOwnerHandle == View.mSelectedActorHandle)) {
                continue;
            }
            const FRenderTemplateGroup& Group{Groups[Object.mTemplateGroupIndex]};
            if (Group.mSky || Group.mPipeline == nullptr || !Group.mPipeline->IsOcclusionCullable(View.mRenderMode)) {
                continue;
            }
            const DirectX::BoundingBox& Box{Object.mWorldAABB};
            if (!std::isfinite(Box.Center.x) || !std::isfinite(Box.Center.y) || !std::isfinite(Box.Center.z) || !std::isfinite(Box.Extents.x) || !std::isfinite(Box.Extents.y) || !std::isfinite(Box.Extents.z) || Box.Extents.x < 0.0f || Box.Extents.y < 0.0f || Box.Extents.z < 0.0f) {
                continue;
            }
            Bounds.mCenter = FVector4{Box.Center.x, Box.Center.y, Box.Center.z, 1.0f};
            Bounds.mExtents = FVector4{Box.Extents.x, Box.Extents.y, Box.Extents.z, 0.0f};
        }
        if (!Resources.mBounds.mBuffer.Update(Context, mBounds.data(), ObjectCount * sizeof(FBounds))) {
            return false;
        }
    }
    mBatches.clear();
    mBatches.reserve(BatchCount);
    for (const FMeshDrawBatch& Item : Items) {
        mBatches.push_back(FBatch{Item.mFirstRecord, Item.mRecordCount, Item.mState.mIndexCount, Item.mState.mFirstIndex});
    }
    if (BatchesGrown || Resources.mBatchSnapshot.size() != mBatches.size() || std::memcmp(Resources.mBatchSnapshot.data(), mBatches.data(), mBatches.size() * sizeof(FBatch)) != 0) {
        if (!Resources.mBatches.mBuffer.Update(Context, mBatches.data(), BatchCount * sizeof(FBatch))) {
            return false;
        }
        Resources.mBatchSnapshot = mBatches;
    }
    const TArray<FMeshDrawRecord>& Records{Queue.GetDrawRecords()};
    if (RecordsGrown || Resources.mRecordSnapshot.size() != RecordCount || std::memcmp(Resources.mRecordSnapshot.data(), Records.data(), RecordCount * sizeof(FMeshDrawRecord)) != 0) {
        if (!Resources.mInputRecords.mBuffer.Update(Context, Records.data(), RecordCount * sizeof(FMeshDrawRecord))) {
            return false;
        }
        Resources.mRecordSnapshot.assign(Records.begin(), Records.begin() + RecordCount);
    }
    Resources.mScene = &Scene;
    Resources.mSceneId = Scene.GetId();
    Resources.mBoundsRevision = Scene.GetRevision();
    Resources.mTemplateRevision = Scene.GetTemplateRevision();
    Resources.mSelectedActorHandle = View.mSelectedActorHandle;
    Resources.mRenderMode = View.mRenderMode;
    Resources.mUseLOD = View.mUseLOD;
    Resources.mRenderSky = View.mSettings.mBRenderSky;
    mConstants = {};
    mConstants.mViewProjection = View.mCamera.mViewProjection;
    mConstants.mSourceWidth = Width;
    mConstants.mSourceHeight = Height;
    mConstants.mDestinationWidth = Resources.mHierarchyWidth;
    mConstants.mDestinationHeight = Resources.mHierarchyHeight;
    mConstants.mObjectCount = ObjectCount;
    mConstants.mRecordCount = RecordCount;
    mConstants.mBatchCount = BatchCount;
    mConstants.mMipCount = static_cast<Uint32>(Resources.mMipResources.size());
    mCurrentResources = &Resources;
    return true;
}

bool FGpuOcclusionCulling::HasHistory() const {
    return mCurrentResources != nullptr && mCurrentResources->mHistoryReady;
}

bool FGpuOcclusionCulling::DispatchPrevious(ID3D11DeviceContext* Context) {
    mDrawReady = false;
    mPreviousReady = false;
    if (Context == nullptr || !HasHistory()) {
        return false;
    }
    mConstants.mViewProjection = mCurrentResources->mPreviousViewProjection;
    mConstants.mCullingPass = 0;
    mPreviousReady = DispatchVisibility(Context);
    return mPreviousReady;
}

bool FGpuOcclusionCulling::DispatchCurrent(ID3D11DeviceContext* Context) {
    mDrawReady = false;
    if (Context == nullptr || mCurrentResources == nullptr || !mPreviousReady) {
        return false;
    }
    mConstants.mViewProjection = mCurrentResources->mViewProjection;
    mConstants.mCullingPass = 1;
    return BuildHierarchy(Context) && DispatchVisibility(Context);
}

bool FGpuOcclusionCulling::CaptureDepth(ID3D11DeviceContext* Context) {
    if (Context == nullptr || mCurrentResources == nullptr) {
        return false;
    }
    mCurrentResources->mHistoryReady = BuildHierarchy(Context);
    if (mCurrentResources->mHistoryReady) {
        mCurrentResources->mPreviousViewProjection = mCurrentResources->mViewProjection;
        mCurrentResources->mHistoryFrame = mFrameSerial;
    }
    return mCurrentResources->mHistoryReady;
}

bool FGpuOcclusionCulling::BuildHierarchy(ID3D11DeviceContext* Context) {
    FViewResources& Resources{*mCurrentResources};
    mConstants.mSourceWidth = static_cast<Uint32>(Resources.mViewport.Width);
    mConstants.mSourceHeight = static_cast<Uint32>(Resources.mViewport.Height);
    mConstants.mDestinationWidth = Resources.mHierarchyWidth;
    mConstants.mDestinationHeight = Resources.mHierarchyHeight;
    Context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11Buffer* ConstantBuffer{mConstantBuffer.GetBuffer()};
    Context->CSSetConstantBuffers(0, 1, &ConstantBuffer);
    Context->CSSetShader(mCopyShader.GetComputeShader(), nullptr, 0);
    Context->CSSetShaderResources(0, 1, Resources.mDepthResource.GetAddressOf());
    Context->CSSetUnorderedAccessViews(0, 1, Resources.mMipOutputs[0].GetAddressOf(), nullptr);
    if (!UploadConstants(Context)) {
        UnbindCompute(Context);
        return false;
    }
    Context->Dispatch((Resources.mHierarchyWidth + 7) / 8, (Resources.mHierarchyHeight + 7) / 8, 1);
    UnbindCompute(Context);
    for (Uint32 Mip{1}; Mip < mConstants.mMipCount; ++Mip) {
        mConstants.mSourceWidth = std::max(Resources.mHierarchyWidth >> (Mip - 1), 1u);
        mConstants.mSourceHeight = std::max(Resources.mHierarchyHeight >> (Mip - 1), 1u);
        mConstants.mDestinationWidth = std::max(Resources.mHierarchyWidth >> Mip, 1u);
        mConstants.mDestinationHeight = std::max(Resources.mHierarchyHeight >> Mip, 1u);
        Context->CSSetConstantBuffers(0, 1, &ConstantBuffer);
        Context->CSSetShader(mReductionShader.GetComputeShader(), nullptr, 0);
        Context->CSSetShaderResources(0, 1, Resources.mMipResources[Mip - 1].GetAddressOf());
        Context->CSSetUnorderedAccessViews(0, 1, Resources.mMipOutputs[Mip].GetAddressOf(), nullptr);
        if (!UploadConstants(Context)) {
            UnbindCompute(Context);
            return false;
        }
        Context->Dispatch((mConstants.mDestinationWidth + 7) / 8, (mConstants.mDestinationHeight + 7) / 8, 1);
        UnbindCompute(Context);
    }
    mConstants.mSourceWidth = static_cast<Uint32>(Resources.mViewport.Width);
    mConstants.mSourceHeight = static_cast<Uint32>(Resources.mViewport.Height);
    return true;
}

bool FGpuOcclusionCulling::DispatchVisibility(ID3D11DeviceContext* Context) {
    FViewResources& Resources{*mCurrentResources};
    Context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11Buffer* ConstantBuffer{mConstantBuffer.GetBuffer()};
    Context->CSSetConstantBuffers(0, 1, &ConstantBuffer);
    Context->CSSetShader(mCullShader.GetComputeShader(), nullptr, 0);
    ID3D11ShaderResourceView* CullResources[]{Resources.mHierarchyResource.Get(), Resources.mBounds.mResourceView.Get()};
    Context->CSSetShaderResources(0, 2, CullResources);
    Context->CSSetUnorderedAccessViews(0, 1, Resources.mVisibility.mUnorderedView.GetAddressOf(), nullptr);
    const bool Culled{DispatchGroups(Context, (mConstants.mObjectCount + 63) / 64)};
    UnbindCompute(Context);
    if (!Culled) {
        return false;
    }
    Context->CSSetConstantBuffers(0, 1, &ConstantBuffer);
    Context->CSSetShader(mArgumentsShader.GetComputeShader(), nullptr, 0);
    ID3D11ShaderResourceView* ArgumentResources[]{Resources.mInputRecords.mResourceView.Get(), Resources.mVisibility.mResourceView.Get(), Resources.mBatches.mResourceView.Get()};
    Context->CSSetShaderResources(2, 3, ArgumentResources);
#if ENABLE_INSTANCE
    ID3D11UnorderedAccessView* Outputs[]{Resources.mOutputRecords.mUnorderedView.Get(), Resources.mArguments.mUnorderedView.Get()};
    ID3D11ShaderResourceView* NullResource{nullptr};
    Context->VSSetShaderResources(0, 1, &NullResource);
    Context->PSSetShaderResources(0, 1, &NullResource);
    Context->CSSetUnorderedAccessViews(1, 2, Outputs, nullptr);
    const Uint32 GroupCount{mConstants.mBatchCount};
#else
    Context->CSSetUnorderedAccessViews(2, 1, Resources.mArguments.mUnorderedView.GetAddressOf(), nullptr);
    const Uint32 GroupCount{(mConstants.mRecordCount + 63) / 64};
#endif
    mDrawReady = DispatchGroups(Context, GroupCount);
    UnbindCompute(Context);
    return mDrawReady;
}

void FGpuOcclusionCulling::BindDrawRecords(ID3D11DeviceContext* Context) const {
#if ENABLE_INSTANCE
    if (mDrawReady && mCurrentResources != nullptr) {
        Context->VSSetShaderResources(0, 1, mCurrentResources->mOutputRecords.mResourceView.GetAddressOf());
        Context->PSSetShaderResources(0, 1, mCurrentResources->mOutputRecords.mResourceView.GetAddressOf());
    }
#endif
}

ID3D11Buffer* FGpuOcclusionCulling::GetArguments() const {
    return mDrawReady && mCurrentResources != nullptr ? mCurrentResources->mArguments.mBuffer.GetBuffer() : nullptr;
}

bool FGpuOcclusionCulling::IsRetest() const {
    return mConstants.mCullingPass != 0;
}

bool FGpuOcclusionCulling::CreateHierarchyResources(ID3D11Device* Device, FViewResources& Resources, Uint32 Width, Uint32 Height) {
    if (Resources.mHierarchyResource != nullptr && Resources.mViewport.Width == static_cast<float>(Width) && Resources.mViewport.Height == static_cast<float>(Height)) {
        return true;
    }
    Resources.mHistoryReady = false;
    Resources.mDepthResource.Reset();
    Resources.mHierarchyResource.Reset();
    Resources.mHierarchy.Reset();
    Resources.mMipResources.clear();
    Resources.mMipOutputs.clear();
    D3D11_TEXTURE2D_DESC Description{};
    Description.ArraySize = 1;
    Description.SampleDesc.Count = 1;
    Description.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SHADER_RESOURCE_VIEW_DESC ResourceDescription{};
    ResourceDescription.Format = DXGI_FORMAT_R32_FLOAT;
    ResourceDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    Resources.mHierarchyWidth = std::bit_ceil(Width);
    Resources.mHierarchyHeight = std::bit_ceil(Height);
    const Uint32 MipCount{static_cast<Uint32>(std::bit_width(std::max(Resources.mHierarchyWidth, Resources.mHierarchyHeight)))};
    Description.Width = Resources.mHierarchyWidth;
    Description.Height = Resources.mHierarchyHeight;
    Description.MipLevels = MipCount;
    Description.Format = DXGI_FORMAT_R32_FLOAT;
    Description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    if (FAILED(Device->CreateTexture2D(&Description, nullptr, Resources.mHierarchy.GetAddressOf()))) {
        return false;
    }
    ResourceDescription.Texture2D.MipLevels = MipCount;
    if (FAILED(Device->CreateShaderResourceView(Resources.mHierarchy.Get(), &ResourceDescription, Resources.mHierarchyResource.GetAddressOf()))) {
        return false;
    }
    Resources.mMipResources.resize(MipCount);
    Resources.mMipOutputs.resize(MipCount);
    for (Uint32 Mip{}; Mip < MipCount; ++Mip) {
        ResourceDescription.Texture2D.MostDetailedMip = Mip;
        ResourceDescription.Texture2D.MipLevels = 1;
        D3D11_UNORDERED_ACCESS_VIEW_DESC OutputDescription{};
        OutputDescription.Format = DXGI_FORMAT_R32_FLOAT;
        OutputDescription.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        OutputDescription.Texture2D.MipSlice = Mip;
        if (FAILED(Device->CreateShaderResourceView(Resources.mHierarchy.Get(), &ResourceDescription, Resources.mMipResources[Mip].GetAddressOf())) || FAILED(Device->CreateUnorderedAccessView(Resources.mHierarchy.Get(), &OutputDescription, Resources.mMipOutputs[Mip].GetAddressOf()))) {
            Resources.mHierarchyResource.Reset();
            return false;
        }
    }
    Resources.mViewport = D3D11_VIEWPORT{0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f};
    return true;
}

bool FGpuOcclusionCulling::EnsureBuffer(ID3D11Device* Device, FBuffer& Buffer, Uint32 Count, Uint32 Stride, bool Writable, bool Indirect) {
    if (Count <= Buffer.mCapacity) {
        return true;
    }
    const Uint32 Capacity{Count <= 0x40000000u ? std::bit_ceil(Count) : Count};
    if (Stride == 0 || Capacity > UINT32_MAX / Stride) {
        return false;
    }
    FBuffer Replacement{};
    FGraphicsBufferDescription Description{};
    Description.mByteSize = Capacity * Stride;
    Description.mStride = Indirect ? 0 : Stride;
    Description.mBindFlags = Indirect ? D3D11_BIND_UNORDERED_ACCESS : D3D11_BIND_SHADER_RESOURCE | (Writable ? D3D11_BIND_UNORDERED_ACCESS : 0);
    Description.mMiscFlags = Indirect ? D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS : D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    if (!Replacement.mBuffer.Initialize(Device, Description)) {
        return false;
    }
    if (!Indirect) {
        D3D11_SHADER_RESOURCE_VIEW_DESC ResourceDescription{};
        ResourceDescription.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        ResourceDescription.Buffer.NumElements = Capacity;
        if (FAILED(Device->CreateShaderResourceView(Replacement.mBuffer.GetBuffer(), &ResourceDescription, Replacement.mResourceView.GetAddressOf()))) {
            return false;
        }
    }
    if (Writable) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC OutputDescription{};
        OutputDescription.Format = Indirect ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
        OutputDescription.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        OutputDescription.Buffer.NumElements = Indirect ? Description.mByteSize / sizeof(Uint32) : Capacity;
        OutputDescription.Buffer.Flags = Indirect ? D3D11_BUFFER_UAV_FLAG_RAW : 0;
        if (FAILED(Device->CreateUnorderedAccessView(Replacement.mBuffer.GetBuffer(), &OutputDescription, Replacement.mUnorderedView.GetAddressOf()))) {
            return false;
        }
    }
    Replacement.mCapacity = Capacity;
    Buffer = std::move(Replacement);
    return true;
}

bool FGpuOcclusionCulling::UploadConstants(ID3D11DeviceContext* Context) {
    return mConstantBuffer.WriteDiscard(Context, &mConstants, sizeof(mConstants));
}

bool FGpuOcclusionCulling::DispatchGroups(ID3D11DeviceContext* Context, Uint32 GroupCount) {
    mConstants.mDispatchWidth = std::min(GroupCount, static_cast<Uint32>(D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION));
    if (mConstants.mDispatchWidth == 0 || !UploadConstants(Context)) {
        return false;
    }
    Context->Dispatch(mConstants.mDispatchWidth, (GroupCount + mConstants.mDispatchWidth - 1) / mConstants.mDispatchWidth, 1);
    return true;
}

void FGpuOcclusionCulling::UnbindCompute(ID3D11DeviceContext* Context) const {
    ID3D11ShaderResourceView* NullResources[5]{};
    ID3D11UnorderedAccessView* NullOutputs[3]{};
    ID3D11Buffer* NullBuffer{nullptr};
    Context->CSSetShaderResources(0, 5, NullResources);
    Context->CSSetUnorderedAccessViews(0, 3, NullOutputs, nullptr);
    Context->CSSetConstantBuffers(0, 1, &NullBuffer);
    Context->CSSetShader(nullptr, nullptr, 0);
}
