#pragma once

#include <d3d11.h>

#include "Math/FMath.h"
#include "Core/STL.h"
#include "Core/Base/FRenderProbe.h"
#include "Asset/IRenderAssetRegistry.h"

class FFrameResource;

class FTextRenderer {
public:
    bool Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity = 256);
    void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource, const TArray<FTextProbe>& TextProbes, IRenderAssetRegistry* AssetRegistry);

private:
    ID3D11Device* mDevice{nullptr};
};
