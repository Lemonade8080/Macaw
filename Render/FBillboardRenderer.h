#pragma once
#include <d3d11.h>
#include <wrl/client.h>

#include "Math/FMath.h"
#include "Core/STL.h"
#include "Core/Base/FRenderProbe.h"
#include "Core/Asset/IAssetRegistry.h"

class FFrameResource;

enum class ERenderMode : std::size_t;
class UPipeline;
class UTexture;

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
    struct FBatchKey {
        FAssetHandle mPipelineHandle{};
        FAssetHandle mTextureHandle{};

        bool operator==(const FBatchKey& Other) const;
    };

    struct FBatchKeyHash {
        std::size_t operator()(const FBatchKey& Key) const noexcept;
    };

    struct FBillboardDraw {
        const UPipeline* mPipeline{};
        const UTexture* mTexture{};
        Uint32 mFirstInstance{};
        Uint32 mInstanceCount{};
    };

    struct FBillboardBatch {
        FBillboardDraw mDraw{};
        Uint32 mWriteCount{};
    };

    static_assert(sizeof(FBillboardData) == 112);

public:
    FBillboardRenderer() = default;
    ~FBillboardRenderer() = default;

public:
    bool Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity = 256);
    void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource, const TArray<FBillboardProbe>& BillboardProbe, const IAssetRegistry* AssetRegistry, ERenderMode Mode);

private:
    ID3D11Device* mDevice{nullptr};
    TArray<FBillboardData> mInstances{};
    TArray<FBillboardDraw> mDraws{};
};
