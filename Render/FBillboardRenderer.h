#pragma once
#include <d3d11.h>
#include <wrl/client.h>

#include "Math/FMath.h"
#include "Core/STL.h"
#include "Core/Base/FRenderProbe.h"
#include "Core/Asset/IAssetRegistry.h"

class FFrameResource;

enum class ERenderMode : std::size_t;

struct FBillboardData {
    FMatrix mWorld{};
    FVector2 mSize{};
    FVector2 mUvMin{};
    FVector2 mUvMax{};
    FVector2 mPad{};
    FVector4 mColor{1.0f, 1.0f, 1.0f, 1.0f};
};

class FBillboardRenderer {
public:
    FBillboardRenderer() = default;
    ~FBillboardRenderer() = default;

public:
    bool Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity = 256);
    void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource, const TArray<FBillboardProbe>& BillboardProbe, const IAssetRegistry* AssetRegistry, ERenderMode Mode);

private:
    ID3D11Device* mDevice{nullptr};

};
