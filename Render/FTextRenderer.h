#pragma once

#include <d3d11.h>

#include "Math/FMath.h"
#include "Core/STL.h"
#include "Core/Base/FRenderProbe.h"
#include "Asset/IRenderAssetRegistry.h"

class FFrameResource;
class UPipeline;

class FTextRenderer {
private:
    struct FTextContext {
        FMatrix mWorld{};
        FVector4 mColor{};
        FVector3 mScreenBoundsExtent{};
        float mScreenUpPadding{};
    };

    struct FGlyphVertex {
        FTextVertex mGlyph{};
        Uint32 mTextIndex{};
    };

    struct FTextDraw {
        const UPipeline* mPipeline{};
        ID3D11ShaderResourceView* mAtlas{};
        Uint32 mFirstVertex{};
        Uint32 mVertexCount{};
    };

    static_assert(sizeof(FTextContext) == 96);
    static_assert(sizeof(FGlyphVertex) == 36);

public:
    bool Initialize(ID3D11Device* InDevice, std::uint32_t InitialCapacity = 256);
    void Render(ID3D11DeviceContext* Context, FFrameResource& FrameResource, const TArray<FTextProbe>& TextProbes, IRenderAssetRegistry* AssetRegistry);

private:
    ID3D11Device* mDevice{nullptr};
    TArray<FTextContext> mTextContexts{};
    TArray<FGlyphVertex> mVertices{};
    TArray<FTextDraw> mDraws{};
};
