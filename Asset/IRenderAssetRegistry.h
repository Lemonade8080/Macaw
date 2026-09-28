#pragma once

#include "Core/Asset/IAssetRegistry.h"

struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

class IRenderAssetRegistry : public IAssetRegistry {
public:
    ~IRenderAssetRegistry() override = default;

public:
    virtual void FlushMaterialBuffer(ID3D11DeviceContext* Context) = 0;
    virtual ID3D11ShaderResourceView* GetMaterialBufferSRV() const = 0;
    virtual void FlushFontAtlas(FAssetHandle Handle, ID3D11DeviceContext* Context) = 0;
};
