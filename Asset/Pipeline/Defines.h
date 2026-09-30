#pragma once
#include <filesystem>

enum class EVertexFormat {
    Float2,
    Float3,
    Float4,
    Uint
};

enum class EInputClassification {
    PerVertex,
    PerInstance
};

enum class EPrimitiveTopology {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

enum class EFillMode {
    Solid,
    Wireframe
};

enum class ECullMode {
    None,
    Front,
    Back
};

enum class ECompareFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class EBlend {
    Zero,
    One,
    SrcAlpha,
    InvSrcAlpha,
    DestAlpha,
    InvDestAlpha,
    SrcColor,
    InvSrcColor,
    DestColor,
    InvDestColor
};

enum class EBlendOp {
    Add,
    Subtract,
    RevSubtract,
    Min,
    Max
};

enum class EStencillOp {
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    IncrementWrap,
    DecrementClamp,
    DecrementWrap,
    Invert,
    Max
};

struct FInputElementDescription {
    std::string mSemanticName{};
    UINT mSemanticIndex{0};
    EVertexFormat mFormat{EVertexFormat::Float3};
    UINT mInputSlot{0};
    UINT mAlignedByteOffset{0};
    EInputClassification mInputClassification{EInputClassification::PerVertex};
    UINT mInstanceDataStepRate{0};
};

struct FRasterizerDescription {
    EFillMode mFillMode{EFillMode::Solid};
    ECullMode mCullMode{ECullMode::Back};
    bool mFrontCounterClockwise{false};
    bool mDepthClipEnable{true};
    bool mScissorEnable{false};
};

struct FDepthStencilDescription {
    bool mDepthEnable{true};
    bool mDepthWriteEnable{true};
    ECompareFunc mDepthFunc{ECompareFunc::LessEqual};

    bool mStencilEnable{true};
    Uint8 mStencilReadMask{255};
    Uint8 mStencilWriteMask{255};

    ECompareFunc mStencilFunc{ECompareFunc::Always};
    EStencillOp mStencilPassOp{EStencillOp::Replace};
    EStencillOp mStencilFailOp{EStencillOp::Keep};
    EStencillOp mStencilDepthFailOp{EStencillOp::Keep};
};

struct FBlendDescription {
    bool mBlendEnable{false};
    EBlend mSrcBlend{EBlend::One};
    EBlend mDestBlend{EBlend::Zero};
    EBlendOp mBlendOp{EBlendOp::Add};
    EBlend mSrcBlendAlpha{EBlend::One};
    EBlend mDestBlendAlpha{EBlend::Zero};
    EBlendOp mBlendOpAlpha{EBlendOp::Add};
};

enum class EShaderStage {
    Vertex,
    Pixel,
    Geometry,
    Hull,
    Domain,
    Compute
};

struct FShaderDescription {
    std::filesystem::path mSource{};
    std::string mEntryPoint{};
    std::string mProfile{};
    EShaderStage mStage{EShaderStage::Vertex};
};

struct FPipelineDescription {
    FShaderDescription mVertexShader{};
    FShaderDescription mPixelShader{};

    FShaderDescription mGeometryShader{};
    bool mBHasGeometryShader{false};

    std::vector<FInputElementDescription> mInputLayout{};

    EPrimitiveTopology mPrimitiveTopology{EPrimitiveTopology::TriangleList};

    FRasterizerDescription mRasterizer{};
    FDepthStencilDescription mDepthStencil{};
    FBlendDescription mBlend{};
    bool mOcclusionCullable{};
    bool mOcclusionOccluder{};
};
