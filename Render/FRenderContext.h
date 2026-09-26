#pragma once

#include <d3d11.h>
#include "Core/Common.h"

class FAssetRegistry;

struct FRenderContext {
    ID3D11Device* mDevice{nullptr};
    ID3D11DeviceContext* mDeviceContext{nullptr};
    FAssetRegistry* mAssetRegistry{nullptr};
    ID3D11ShaderResourceView* mLightResource{nullptr};
    Uint32 mLightCount{};
    Uint32 mAnimationFrame{};
};
