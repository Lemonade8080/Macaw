#include "pch.h"
#include "FMaterialBuffer.h"
#include "UMaterial.h"

bool FMaterialBuffer::Initialize(ID3D11Device* Device, Uint32 InMaxMaterialCount) {
    if (Device == nullptr || InMaxMaterialCount == 0) {
        return false;
    }

    mMaxMaterialCount = InMaxMaterialCount;

    mSlots.resize(mMaxMaterialCount);
    mMaterials.resize(mMaxMaterialCount);

    mFreeIndices.reserve(mMaxMaterialCount);

    for (Uint32 Index{0}; Index < mMaxMaterialCount; ++Index) {
        mFreeIndices.push_back(mMaxMaterialCount - Index - 1);
    }

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth = MaterialGpuStride * mMaxMaterialCount;
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.CPUAccessFlags = 0;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = MaterialGpuStride;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = mSlots.data();

    HRESULT Result{Device->CreateBuffer(&BufferDesc, &InitialData, mBuffer.GetAddressOf())};

    if (FAILED(Result)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
    SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.FirstElement = 0;
    SRVDesc.Buffer.NumElements = mMaxMaterialCount;

    Result = Device->CreateShaderResourceView(mBuffer.Get(), &SRVDesc, mSrv.GetAddressOf());

    return SUCCEEDED(Result);
}

bool FMaterialBuffer::RegisterMaterial(UMaterial* Material) {
    if (Material == nullptr || Material->GetGPUDataCount() > mFreeIndices.size()) {
        return false;
    }

    Material->mGpuIndices.clear();
    Material->mGpuIndices.reserve(Material->GetGPUDataCount());

    for (Uint32 GroupIndex{0}; GroupIndex < Material->GetGPUDataCount(); ++GroupIndex) {
        const Uint32 Index{AllocateSlot()};
        mMaterials[Index] = FMaterialBufferEntry{Material, GroupIndex};
        Material->mGpuIndices.push_back(Index);
    }

    Material->MarkGPUDataDirty();

    return true;
}

void FMaterialBuffer::UnregisterMaterial(UMaterial* Material) {
    if (Material == nullptr || Material->mGpuIndices.empty()) {
        return;
    }

    for (const Uint32 Index : Material->mGpuIndices) {
        if (Index >= mMaterials.size() || mMaterials[Index].mMaterial != Material) {
            continue;
        }

        mMaterials[Index] = {};
        mSlots[Index] = {};
        ReleaseSlot(Index);
    }

    Material->mGpuIndices.clear();
    ++Material->mRenderRevision;
    Material->mBGpuDataDirty = false;
}

void FMaterialBuffer::Flush(ID3D11DeviceContext* DeviceContext) {
    if (DeviceContext == nullptr || mBuffer == nullptr) {
        return;
    }

    for (Uint32 Index{0}; Index < mMaxMaterialCount; ++Index) {
        const FMaterialBufferEntry& Entry{mMaterials[Index]};
        UMaterial* Material{Entry.mMaterial};

        if (Material == nullptr || !Material->mBGpuDataDirty) {
            continue;
        }

        Material->BuildGPUData(Entry.mGroupIndex, mSlots[Index]);

        const Uint32 Offset{Index * MaterialGpuStride};

        D3D11_BOX Box{};
        Box.left = Offset;
        Box.right = Offset + MaterialGpuStride;
        Box.top = 0;
        Box.bottom = 1;
        Box.front = 0;
        Box.back = 1;

        DeviceContext->UpdateSubresource(mBuffer.Get(), 0, &Box, mSlots[Index].mData.data(), 0, 0);
    }

    for (const FMaterialBufferEntry& Entry : mMaterials) {
        if (Entry.mMaterial != nullptr) {
            Entry.mMaterial->mBGpuDataDirty = false;
        }
    }
}

Uint32 FMaterialBuffer::AllocateSlot() {
    const Uint32 Index{mFreeIndices.back()};
    mFreeIndices.pop_back();

    return Index;
}

void FMaterialBuffer::ReleaseSlot(Uint32 Index) {
    mFreeIndices.push_back(Index);
}

ID3D11Buffer* FMaterialBuffer::GetBuffer() const {
    return mBuffer.Get();
}

ID3D11ShaderResourceView* const* FMaterialBuffer::GetSRV() const {
    return mSrv.GetAddressOf();
}

Uint32 FMaterialBuffer::GetMaxMaterialCount() const {
    return mMaxMaterialCount;
}

void FMaterialBuffer::Reset() {
    mMaxMaterialCount = 0;
    mSlots.clear();
    mMaterials.clear();
    mFreeIndices.clear();
    mBuffer.Reset();
    mSrv.Reset();
}
