#pragma once

#include <d3d11.h>

#include "Math/FMath.h"
#include "Core/STL.h"
#include "Core/Base/FRenderProbe.h"
#include "Render/Buffer/FGraphicsBuffer.h"
#include "Render/Buffer/TGraphicsRootConstants.h"
#include "Asset/IRenderAssetRegistry.h"

class FTextRenderer {
private:
    struct FTextConstants {
        FMatrix mWorld{};
        FMatrix mViewwProjection{};
        FMatrix mCameraWorld{};
        FVector4 mColor{1.0f, 1.0f, 1.0f, 1.0f};
        FVector3 mScreenBoundsExtent{};
        float mScreenUpPadding{};
    };

    static_assert(sizeof(FTextConstants) == sizeof(std::uint32_t) * 56);

public:
    bool Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity = 256);
    void Render(ID3D11DeviceContext* Context, const TArray<FTextProbe>& TextProbes, const CameraProbe& Camera, IRenderAssetRegistry* AssetRegistry);

private:
    bool EnsureCapacity(std::uint32_t ReauiredCapacity);

private:
    ID3D11Device* mDevice{nullptr};
    FGraphicsBuffer mVertexBuffer{};
    std::uint32_t mVertexCapacity{0};
    TGraphicsRootConstants<56> mTextConstants{};
};
