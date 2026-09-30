#pragma once

#include <cstdint>
#include <d3d11.h>

class IRenderSurface {
public:
    virtual ~IRenderSurface() = default;

public:
    virtual bool Resize(ID3D11Device* Device, std::uint32_t Width, std::uint32_t Height) = 0;
    virtual void Bind(ID3D11DeviceContext* Context) const = 0;
    virtual void Clear(ID3D11DeviceContext* Context, const float ClearColor[4]) const = 0;
    virtual void ClearDepth(ID3D11DeviceContext* Context) const = 0;
    virtual void Reset() = 0;
    virtual bool IsValid() const = 0;
    virtual const D3D11_VIEWPORT& GetViewport() const = 0;
    virtual ID3D11ShaderResourceView* GetShaderResourceView() const = 0;
    virtual ID3D11ShaderResourceView* GetDepthShaderResourceView() const = 0;
};
