#pragma once

#include "FRenderContext.h"
#include "FRenderQueue.h"
#include "Buffer/TGraphicsArray.h"
#include "Buffer/TGraphicsRootConstants.h"

class FMeshRenderer {
private:
    struct FModelContext {
        FMatrix mWorld{};
        Uint32 mMaterialIndex{UINT32_MAX};
        Uint32 mFlags{};
    };

public:
    bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context);
    void Draw(const FRenderContext& Context, const TArray<FMeshDrawItem>& Items, const CameraProbe& Camera, ERenderMode Mode);
    void Reset();

private:
    TGraphicsArray<FModelContext, true, true> mModelContextArray{};
    TGraphicsRootConstants<64> mRootConstants{};
    TArray<FModelContext> mModelContexts{};
};
