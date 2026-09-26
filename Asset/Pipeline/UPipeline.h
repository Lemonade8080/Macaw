#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <array>
#include <string>
#include <vector>

#include "FShader.h"

#include "Defines.h"
#include "Asset/UAsset.h"
#include "../../Core/Base/TypeInfo.h"
#include "Wrapper.h"

struct PipelineUnit {
    FShader mVertexShader{};
    FShader mPixelShader{};
    FShader mGeometryShader{};
    Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout{};
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> mRasterizerState{};
    Microsoft::WRL::ComPtr<ID3D11BlendState> mBlendState{};
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthStencilState{};
    D3D11_PRIMITIVE_TOPOLOGY mPrimitiveTopology{D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
    bool mInitialized{false};
    UINT mStencilRef{0};
};

enum class ERenderMode : std::size_t {
    Lit,
    Outline,
    Unlit,
    Wireframe,
    LitWireframe,
    Max
};

class UPipeline : public UAsset {
public:
    UPipeline() = default;
    ~UPipeline() = default;

    UPipeline(const UPipeline&) = delete;
    UPipeline& operator=(const UPipeline&) = delete;

    UPipeline(UPipeline&&) noexcept = default;
    UPipeline& operator=(UPipeline&&) noexcept = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UPipeline, UAsset);

    bool Initialize(ID3D11Device* Device, const std::filesystem::path& PipelinePath);

    void Bind(ID3D11DeviceContext* Context) const;
    void Bind(ID3D11DeviceContext* Context, ERenderMode Mode, UINT StencilReference = 1) const;
    void Reset();

    void SetRenderMode(ERenderMode Mode);
    bool RenderModeSettable(ERenderMode Mode);
    ERenderMode GetRenderMode() const;
    ERenderMode ResolveRenderMode(ERenderMode Mode) const;

private:
    bool InitializeFamily(ID3D11Device* Device, const std::filesystem::path& FamilyDirectory);
    bool InitializeModes(ID3D11Device* Device, const std::array<std::filesystem::path, static_cast<std::size_t>(ERenderMode::Max)>& ModePaths);
    bool LoadPipelineDescription(const std::filesystem::path& Path, FPipelineDescription& OutDescription);

    bool Make(ID3D11Device* Device, const FPipelineDescription& Description, PipelineUnit& PipelineUnit);
    virtual void Serialize(FArchive& Ar) override;

private:
    std::filesystem::path mOptionFilePath{};

    std::size_t mModeIndex{0};

    std::vector<PipelineUnit> mPipelines{};

    std::size_t mPrimaryIndex{0};
};
