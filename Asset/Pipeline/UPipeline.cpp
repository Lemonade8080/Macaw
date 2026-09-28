#include "pch.h"
#include "UPipeline.h"

#include "Core/Base/ErrorHandler.h"

#include <memory>

namespace {
    EStencillOp ParseStencilOperation(const char* Value) {
        constexpr std::array Names{"Keep", "Zero", "Replace", "IncrementClamp", "IncrementWrap", "DecrementClamp", "DecrementWrap", "Invert"};
        for (std::size_t Index{}; Index < Names.size(); ++Index) {
            if (std::strcmp(Value, Names[Index]) == 0) {
                return static_cast<EStencillOp>(Index);
            }
        }
        ErrorHandler::Report("ParseStencilOperation", "The stencil operation is invalid.", ErrorHandler::EErrorLevel::Error);
        return EStencillOp::Keep;
    }
}

bool UPipeline::Initialize(ID3D11Device* Device, const std::filesystem::path& PipelinePath) {
    if (std::filesystem::is_directory(PipelinePath)) {
        return InitializeFamily(Device, PipelinePath);
    }

    if (!UAsset::Initialize(Device, PipelinePath)) {
        return false;
    }

    std::array<std::filesystem::path, static_cast<std::size_t>(ERenderMode::Max)> ModePaths{};
    ModePaths[static_cast<std::size_t>(ERenderMode::Lit)] = PipelinePath;
    return InitializeModes(Device, ModePaths);
}

bool UPipeline::InitializeFamily(ID3D11Device* Device, const std::filesystem::path& FamilyDirectory) {
    if (!UAsset::Initialize(Device, FamilyDirectory) || !std::filesystem::is_directory(FamilyDirectory)) {
        return false;
    }

    constexpr std::array<const char*, static_cast<std::size_t>(ERenderMode::Max)> ModeNames{"Lit", "Outline", "Unlit", "Wireframe"};
    std::array<std::filesystem::path, static_cast<std::size_t>(ERenderMode::Max)> ModePaths{};
    const std::string FamilyName{FamilyDirectory.filename().generic_string()};
    for (std::size_t Index{0}; Index < ModeNames.size(); ++Index) {
        const std::filesystem::path Path{FamilyDirectory / (FamilyName + "_" + ModeNames[Index] + ".json")};
        if (std::filesystem::is_regular_file(Path)) {
            ModePaths[Index] = Path;
        }
    }
    if (ModePaths[static_cast<std::size_t>(ERenderMode::Lit)].empty()) {
        return false;
    }
    return InitializeModes(Device, ModePaths);
}

bool UPipeline::InitializeModes(ID3D11Device* Device, const std::array<std::filesystem::path, static_cast<std::size_t>(ERenderMode::Max)>& ModePaths) {
    std::array<FPipelineDescription, static_cast<std::size_t>(ERenderMode::Max)> Descriptions{};
    std::array<bool, static_cast<std::size_t>(ERenderMode::Max)> EnabledModes{};
    EnabledModes.fill(true);
    const std::size_t LitIndex{static_cast<std::size_t>(ERenderMode::Lit)};
    if (!LoadPipelineDescription(ModePaths[LitIndex], Descriptions[LitIndex])) {
        return false;
    }
    for (std::size_t Index{0}; Index < Descriptions.size(); ++Index) {
        if (Index == LitIndex) {
            continue;
        }
        if (!ModePaths[Index].empty()) {
            if (!LoadPipelineDescription(ModePaths[Index], Descriptions[Index])) {
                return false;
            }
            continue;
        }
        Descriptions[Index] = Descriptions[LitIndex];
        if (Descriptions[Index].mPrimitiveTopology != EPrimitiveTopology::TriangleList && Descriptions[Index].mPrimitiveTopology != EPrimitiveTopology::TriangleStrip) {
            continue;
        }
        if (Index == static_cast<std::size_t>(ERenderMode::Outline)) {
            const bool HasNormal{std::ranges::any_of(Descriptions[Index].mInputLayout, [](const FInputElementDescription& Element) {
                return Element.mSemanticName == "NORMAL" && Element.mSemanticIndex == 0;
            })};
            if (!HasNormal) {
                EnabledModes[Index] = false;
                continue;
            }
            Descriptions[Index].mGeometryShader.mSource = "./Content/Shader/Outline.hlsl";
            Descriptions[Index].mGeometryShader.mEntryPoint = "MainGS";
            Descriptions[Index].mGeometryShader.mProfile = "gs_5_0";
            Descriptions[Index].mGeometryShader.mStage = EShaderStage::Geometry;
            Descriptions[Index].mBHasGeometryShader = true;
            Descriptions[Index].mPixelShader.mSource = "./Content/Shader/Outline.hlsl";
            Descriptions[Index].mPixelShader.mEntryPoint = "MainPS";
            Descriptions[Index].mRasterizer.mFillMode = EFillMode::Solid;
            Descriptions[Index].mRasterizer.mCullMode = ECullMode::Front;
            Descriptions[Index].mDepthStencil.mDepthWriteEnable = false;
            Descriptions[Index].mDepthStencil.mStencilEnable = true;
            Descriptions[Index].mDepthStencil.mStencilReadMask = 1;
            Descriptions[Index].mDepthStencil.mStencilWriteMask = 0;
            Descriptions[Index].mDepthStencil.mStencilFunc = ECompareFunc::NotEqual;
            Descriptions[Index].mDepthStencil.mStencilPassOp = EStencillOp::Keep;
            Descriptions[Index].mDepthStencil.mStencilFailOp = EStencillOp::Keep;
            Descriptions[Index].mDepthStencil.mStencilDepthFailOp = EStencillOp::Keep;
        } else if (Index == static_cast<std::size_t>(ERenderMode::Wireframe)) {
            Descriptions[Index].mRasterizer.mFillMode = EFillMode::Wireframe;
        }
    }

    Reset();
    mPipelines.resize(Descriptions.size());
    for (std::size_t Index{0}; Index < Descriptions.size(); ++Index) {
        if (!EnabledModes[Index]) {
            continue;
        }
        if (!Make(Device, Descriptions[Index], mPipelines[Index])) {
            Reset();
            return false;
        }
    }
    mOptionFilePath = ModePaths[LitIndex];
    mPrimaryIndex = LitIndex;
    mModeIndex = LitIndex;
    return true;
}

bool UPipeline::Make(ID3D11Device* Device, const FPipelineDescription& Description, PipelineUnit& Pipeline) {
    if (Device == nullptr) {
        ErrorHandler::Report("Pipeline::Initialize", "A valid Direct3D device is required to initialize a pipeline.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    if (!Pipeline.mVertexShader.Initialize(Device, Description.mVertexShader)) {
        return false;
    }

    if (!Pipeline.mPixelShader.Initialize(Device, Description.mPixelShader)) {
        return false;
    }

    if (Description.mBHasGeometryShader) {
        if (!Pipeline.mGeometryShader.Initialize(Device, Description.mGeometryShader)) {
            return false;
        }
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> NativeInputLayout{};
    NativeInputLayout.reserve(Description.mInputLayout.size());

    for (const FInputElementDescription& Source : Description.mInputLayout) {
        D3D11_INPUT_ELEMENT_DESC Element{};
        Element.SemanticName = Source.mSemanticName.c_str();
        Element.SemanticIndex = Source.mSemanticIndex;
        Element.Format = ConvertVertexFormat(Source.mFormat);
        Element.InputSlot = Source.mInputSlot;
        Element.AlignedByteOffset = Source.mAlignedByteOffset;
        Element.InputSlotClass = Source.mInputClassification == EInputClassification::PerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        Element.InstanceDataStepRate = Source.mInstanceDataStepRate;

        NativeInputLayout.emplace_back(Element);
    }

    HRESULT Result{S_OK};
    if (!NativeInputLayout.empty()) {
        Result = Device->CreateInputLayout(NativeInputLayout.data(), static_cast<UINT>(NativeInputLayout.size()), Pipeline.mVertexShader.GetByteCodeData(), Pipeline.mVertexShader.GetByteCodeSize(), Pipeline.mInputLayout.GetAddressOf());

        if (FAILED(Result)) {
            ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the input layout.", ErrorHandler::EErrorLevel::Error);
            Reset();
            return false;
        }
    } else {
        Pipeline.mInputLayout.Reset();
    }

    D3D11_RASTERIZER_DESC RasterizerDesc{};
    RasterizerDesc.FillMode = ConvertFillMode(Description.mRasterizer.mFillMode);
    RasterizerDesc.CullMode = ConvertCullMode(Description.mRasterizer.mCullMode);
    RasterizerDesc.FrontCounterClockwise = Description.mRasterizer.mFrontCounterClockwise;
    RasterizerDesc.DepthClipEnable = Description.mRasterizer.mDepthClipEnable;
    RasterizerDesc.ScissorEnable = Description.mRasterizer.mScissorEnable;

    Result = Device->CreateRasterizerState(&RasterizerDesc, Pipeline.mRasterizerState.GetAddressOf());

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the rasterizer state.", ErrorHandler::EErrorLevel::Error);
        Reset();
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{};
    DepthStencilDesc.DepthEnable = Description.mDepthStencil.mDepthEnable;
    DepthStencilDesc.DepthWriteMask = Description.mDepthStencil.mDepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.DepthFunc = ConvertCompareFunc(Description.mDepthStencil.mDepthFunc);
    DepthStencilDesc.StencilEnable = Description.mDepthStencil.mStencilEnable;
    DepthStencilDesc.StencilReadMask = Description.mDepthStencil.mStencilReadMask;
    DepthStencilDesc.StencilWriteMask = Description.mDepthStencil.mStencilWriteMask;
    DepthStencilDesc.FrontFace.StencilFunc = ConvertCompareFunc(Description.mDepthStencil.mStencilFunc);
    DepthStencilDesc.FrontFace.StencilPassOp = ConvertStencillOp(Description.mDepthStencil.mStencilPassOp);
    DepthStencilDesc.FrontFace.StencilFailOp = ConvertStencillOp(Description.mDepthStencil.mStencilFailOp);
    DepthStencilDesc.FrontFace.StencilDepthFailOp = ConvertStencillOp(Description.mDepthStencil.mStencilDepthFailOp);

    DepthStencilDesc.BackFace = DepthStencilDesc.FrontFace;

    Result = Device->CreateDepthStencilState(&DepthStencilDesc, Pipeline.mDepthStencilState.GetAddressOf());

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the depth-stencil state.", ErrorHandler::EErrorLevel::Error);
        Reset();
        return false;
    }

    D3D11_BLEND_DESC BlendDesc{};
    BlendDesc.AlphaToCoverageEnable = false;
    BlendDesc.IndependentBlendEnable = false;

    D3D11_RENDER_TARGET_BLEND_DESC& RenderTarget{BlendDesc.RenderTarget[0]};
    RenderTarget.BlendEnable = Description.mBlend.mBlendEnable;
    RenderTarget.SrcBlend = ConvertBlend(Description.mBlend.mSrcBlend);
    RenderTarget.DestBlend = ConvertBlend(Description.mBlend.mDestBlend);
    RenderTarget.BlendOp = ConvertBlendOp(Description.mBlend.mBlendOp);
    RenderTarget.SrcBlendAlpha = ConvertBlend(Description.mBlend.mSrcBlendAlpha);
    RenderTarget.DestBlendAlpha = ConvertBlend(Description.mBlend.mDestBlendAlpha);
    RenderTarget.BlendOpAlpha = ConvertBlendOp(Description.mBlend.mBlendOpAlpha);
    RenderTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    Result = Device->CreateBlendState(&BlendDesc, Pipeline.mBlendState.GetAddressOf());

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Pipeline::Initialize", "Failed to create the blend state.", ErrorHandler::EErrorLevel::Error);
        Reset();
        return false;
    }

    Pipeline.mPrimitiveTopology = ConvertPrimitiveTopology(Description.mPrimitiveTopology);

    Pipeline.mInitialized = true;
    return true;
}

void UPipeline::Bind(ID3D11DeviceContext* Context) const {
    Bind(Context, static_cast<ERenderMode>(mModeIndex));
}

void UPipeline::Bind(ID3D11DeviceContext* Context, ERenderMode Mode, UINT StencilReference) const {
    if (Context == nullptr) {
        ErrorHandler::Report("Pipeline::Bind", "A valid Direct3D device context is required to bind a pipeline.", ErrorHandler::EErrorLevel::Error);
        return;
    }

    const std::size_t Index{static_cast<std::size_t>(Mode)};
    if (Index >= mPipelines.size() || !mPipelines[Index].mInitialized) {
        return;
    }

    Context->IASetInputLayout(mPipelines[Index].mInputLayout.Get());
    Context->IASetPrimitiveTopology(mPipelines[Index].mPrimitiveTopology);

    Context->VSSetShader(mPipelines[Index].mVertexShader.GetVertexShader(), nullptr, 0);
    Context->PSSetShader(mPipelines[Index].mPixelShader.GetPixelShader(), nullptr, 0);

    Context->GSSetShader(mPipelines[Index].mGeometryShader.GetGeometryShader(), nullptr, 0);
    Context->HSSetShader(nullptr, nullptr, 0);
    Context->DSSetShader(nullptr, nullptr, 0);

    Context->RSSetState(mPipelines[Index].mRasterizerState.Get());
    Context->OMSetBlendState(mPipelines[Index].mBlendState.Get(), nullptr, 0xffffffff);
    Context->OMSetDepthStencilState(mPipelines[Index].mDepthStencilState.Get(), StencilReference);
}

void UPipeline::Reset() {
    for (auto& Pipelines : mPipelines) {
        Pipelines.mVertexShader.Reset();
        Pipelines.mPixelShader.Reset();
        Pipelines.mGeometryShader.Reset();

        Pipelines.mInputLayout.Reset();
        Pipelines.mRasterizerState.Reset();
        Pipelines.mBlendState.Reset();
        Pipelines.mDepthStencilState.Reset();

        Pipelines.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        Pipelines.mInitialized = false;
    }
    mPipelines.clear();
    mPrimaryIndex = 0;
    mModeIndex = 0;
}

void UPipeline::SetRenderMode(ERenderMode Mode) {
    const std::size_t RequestedIndex{static_cast<std::size_t>(Mode)};
    if (RequestedIndex < mPipelines.size() && mPipelines[RequestedIndex].mInitialized) {
        mModeIndex = RequestedIndex;
    } else {
        mModeIndex = mPrimaryIndex;
    }
}

bool UPipeline::RenderModeSettable(ERenderMode Mode) const {
    const std::size_t RequestedIndex{static_cast<std::size_t>(Mode)};
    return RequestedIndex < mPipelines.size() && mPipelines[RequestedIndex].mInitialized;
}

ERenderMode UPipeline::GetRenderMode() const {
    return static_cast<ERenderMode>(mModeIndex);
}

bool UPipeline::LoadPipelineDescription(const std::filesystem::path& Path, FPipelineDescription& OutDescription) {
    FILE* File{nullptr};

#ifdef _WIN32
    _wfopen_s(&File, Path.c_str(), L"rb");
#else
    File = std::fopen(Path.string().c_str(), "rb");
#endif

    if (File == nullptr) {
        ErrorHandler::Report("Pipeline::LoadPipelineDescription", "Failed to open the pipeline option file.", ErrorHandler::EErrorLevel::Critical);
        return false;
    }

    std::unique_ptr<char[]> ReadBufferPtr{new char[65536]};
    rapidjson::FileReadStream Stream{File, ReadBufferPtr.get(), sizeof(ReadBufferPtr.get())};

    rapidjson::Document Root{};
    Root.ParseStream(Stream);

    std::fclose(File);

    if (Root.HasParseError() || !Root.IsObject()) {
        ErrorHandler::Report("Pipeline::LoadPipelineDescription", "Failed to parse the pipeline option file as a JSON object.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    FPipelineDescription Description{};

    const rapidjson::Value* VS{GetObject(Root, "VertexShader")};

    if (VS == nullptr) {
        return false;
    }

    const char* VSSource{GetString(*VS, "Source")};
    const char* VSEntryPoint{GetString(*VS, "EntryPoint")};
    const char* VSProfile{GetString(*VS, "Profile")};

    if (VSSource == nullptr || VSEntryPoint == nullptr || VSProfile == nullptr) {
        return false;
    }

    Description.mVertexShader.mSource = VSSource;
    Description.mVertexShader.mEntryPoint = VSEntryPoint;
    Description.mVertexShader.mProfile = VSProfile;
    Description.mVertexShader.mStage = EShaderStage::Vertex;

    const rapidjson::Value* PS{GetObject(Root, "PixelShader")};

    if (PS == nullptr) {
        return false;
    }

    const char* PSSource{GetString(*PS, "Source")};
    const char* PSEntryPoint{GetString(*PS, "EntryPoint")};
    const char* PSProfile{GetString(*PS, "Profile")};

    if (PSSource == nullptr || PSEntryPoint == nullptr || PSProfile == nullptr) {
        return false;
    }

    Description.mPixelShader.mSource = PSSource;
    Description.mPixelShader.mEntryPoint = PSEntryPoint;
    Description.mPixelShader.mProfile = PSProfile;
    Description.mPixelShader.mStage = EShaderStage::Pixel;

    if (Root.HasMember("GeometryShader")) {
        const rapidjson::Value& GS{Root["GeometryShader"]};

        if (!GS.IsObject()) {
            return false;
        }

        const char* GSSource{GetString(GS, "Source")};
        const char* GSEntryPoint{GetString(GS, "EntryPoint")};
        const char* GSProfile{GetString(GS, "Profile")};

        if (GSSource == nullptr || GSEntryPoint == nullptr || GSProfile == nullptr) {
            return false;
        }

        Description.mGeometryShader.mSource = GSSource;
        Description.mGeometryShader.mEntryPoint = GSEntryPoint;
        Description.mGeometryShader.mProfile = GSProfile;
        Description.mGeometryShader.mStage = EShaderStage::Geometry;
        Description.mBHasGeometryShader = true;
    }

    const rapidjson::Value* InputLayout{GetArray(Root, "InputLayout")};

    if (InputLayout == nullptr) {
        return false;
    }

    Description.mInputLayout.reserve(InputLayout->Size());

    for (const rapidjson::Value& Element : InputLayout->GetArray()) {
        if (!Element.IsObject()) {
            ErrorHandler::Report("Pipeline::LoadPipelineDescription", "Each input-layout element must be a JSON object.", ErrorHandler::EErrorLevel::Error);
            return false;
        }

        const char* SemanticName{GetString(Element, "SemanticName")};
        const char* Format{GetString(Element, "Format")};

        if (SemanticName == nullptr || Format == nullptr) {
            return false;
        }

        FInputElementDescription Input{};
        Input.mSemanticName = SemanticName;
        Input.mSemanticIndex = GetUint(Element, "SemanticIndex", 0);
        Input.mFormat = ParseVertexFormat(Format);
        Input.mInputSlot = GetUint(Element, "InputSlot", 0);
        Input.mAlignedByteOffset = GetUint(Element, "AlignedByteOffset");
        Input.mInputClassification = std::strcmp(GetString(Element, "InputClassification", "PerVertex"), "PerInstance") == 0 ? EInputClassification::PerInstance : EInputClassification::PerVertex;
        Input.mInstanceDataStepRate = GetUint(Element, "InstanceDataStepRate", 0);

        Description.mInputLayout.emplace_back(std::move(Input));
    }

    const char* PrimitiveTopology{GetString(Root, "PrimitiveTopology")};

    if (PrimitiveTopology == nullptr) {
        return false;
    }

    Description.mPrimitiveTopology = ParsePrimitiveTopology(PrimitiveTopology);

    const rapidjson::Value* Rasterizer{GetObject(Root, "Rasterizer")};

    if (Rasterizer == nullptr) {
        return false;
    }

    const char* FillMode{GetString(*Rasterizer, "FillMode")};
    const char* CullMode{GetString(*Rasterizer, "CullMode")};

    if (FillMode == nullptr || CullMode == nullptr) {
        return false;
    }

    Description.mRasterizer.mFillMode = ParseFillMode(FillMode);
    Description.mRasterizer.mCullMode = ParseCullMode(CullMode);
    Description.mRasterizer.mFrontCounterClockwise = GetBool(*Rasterizer, "FrontCounterClockwise", false);
    Description.mRasterizer.mDepthClipEnable = GetBool(*Rasterizer, "DepthClipEnable", true);
    Description.mRasterizer.mScissorEnable = GetBool(*Rasterizer, "ScissorEnable", false);

    const rapidjson::Value* DepthStencil{GetObject(Root, "DepthStencil")};

    if (DepthStencil == nullptr) {
        return false;
    }

    Description.mDepthStencil.mDepthEnable = GetBool(*DepthStencil, "DepthEnable", true);
    Description.mDepthStencil.mDepthWriteEnable = GetBool(*DepthStencil, "DepthWriteEnable", true);
    Description.mDepthStencil.mDepthFunc = ParseCompareFunc(GetString(*DepthStencil, "DepthFunc", "LessEqual"));
    Description.mDepthStencil.mStencilEnable = GetBool(*DepthStencil, "StencilEnable", true);
    Description.mDepthStencil.mStencilReadMask = static_cast<Uint8>(GetUint(*DepthStencil, "StencilReadMask", 255));
    Description.mDepthStencil.mStencilWriteMask = static_cast<Uint8>(GetUint(*DepthStencil, "StencilWriteMask", 255));
    Description.mDepthStencil.mStencilFunc = ParseCompareFunc(GetString(*DepthStencil, "StencilFunc", "Always"));
    Description.mDepthStencil.mStencilPassOp = ParseStencilOperation(GetString(*DepthStencil, "StencilPassOp", "Replace"));
    Description.mDepthStencil.mStencilFailOp = ParseStencilOperation(GetString(*DepthStencil, "StencilFailOp", "Keep"));
    Description.mDepthStencil.mStencilDepthFailOp = ParseStencilOperation(GetString(*DepthStencil, "StencilDepthFailOp", "Keep"));

    const rapidjson::Value* Blend{GetObject(Root, "Blend")};

    if (Blend == nullptr) {
        return false;
    }

    Description.mBlend.mBlendEnable = GetBool(*Blend, "BlendEnable", false);
    Description.mBlend.mSrcBlend = ParseBlend(GetString(*Blend, "SrcBlend", "One"));
    Description.mBlend.mDestBlend = ParseBlend(GetString(*Blend, "DestBlend", "Zero"));
    Description.mBlend.mBlendOp = ParseBlendOp(GetString(*Blend, "BlendOp", "Add"));
    Description.mBlend.mSrcBlendAlpha = ParseBlend(GetString(*Blend, "SrcBlendAlpha", "One"));
    Description.mBlend.mDestBlendAlpha = ParseBlend(GetString(*Blend, "DestBlendAlpha", "Zero"));
    Description.mBlend.mBlendOpAlpha = ParseBlendOp(GetString(*Blend, "BlendOpAlpha", "Add"));

    OutDescription = std::move(Description);

    return true;
}

void UPipeline::Serialize(FArchive& Ar) {
    UAsset::Serialize(Ar);
}

ERenderMode UPipeline::ResolveRenderMode(ERenderMode Mode) const {
    const std::size_t Index{static_cast<std::size_t>(Mode)};
    return Index < mPipelines.size() && mPipelines[Index].mInitialized ? Mode : static_cast<ERenderMode>(mPrimaryIndex);
}
