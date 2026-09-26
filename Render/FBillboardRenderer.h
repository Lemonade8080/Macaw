#pragma once
#include <d3d11.h>
#include <wrl/client.h>

#include "Math/FMath.h"
#include "Core/STL.h"
#include "Core/Base/FRenderProbe.h"
#include "Render/Buffer/FGraphicsBuffer.h"
#include "Render/Buffer/TGraphicsRootConstants.h"
#include "Asset/FAssetRegistry.h"

struct FBillboardData {
    FMatrix mWorld{};
    FVector2 mSize{};
    FVector2 mUvMin{};
    FVector2 mUvMax{};
    FVector2 mPad{};
    FVector4 mColor{1.0f, 1.0f, 1.0f, 1.0f};
};

class FBillboardRenderer {
private:
    struct FBillboardViewConstans {
        FMatrix mViewProjection{};
        FMatrix mCameraWorld{};
    };

    static_assert(sizeof(FBillboardViewConstans) == sizeof(Uint32) * 32);

public:
    FBillboardRenderer() = default;
    ~FBillboardRenderer() = default;

public:
    bool Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity = 256);
    void Render(ID3D11DeviceContext* Context, const TArray<FBillboardProbe>& BillboardProbe, const CameraProbe& Camera, FAssetRegistry* AssetRegistry, ERenderMode Mode);

private:
    bool EnsureCapacity(Uint32 RequiredCapacity);

private:
    ID3D11Device* mDevice{nullptr};

    FGraphicsBuffer mInstanceBuffer{};
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mInstanceBufferSrv{};
    Uint32 mInstanceCapacity{0};
    TGraphicsRootConstants<32> mViewConstantBuffer{};
};
