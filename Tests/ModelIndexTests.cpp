#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <rapidjson/document.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

struct FTestModelContext {
    float mWorld[16]{};
    UINT mMaterialIndex{};
    UINT mFlags{};
};

struct FTestVertexOutput {
    float mPosition[4]{};
    UINT mMaterialIndex{};
};

struct FTestDraw {
    UINT mModelIndex{};
    UINT mInstanceCount{};
    bool mInstanced{};
};

void CheckResult(HRESULT Result) {
    if (FAILED(Result)) {
        throw std::runtime_error{std::to_string(static_cast<unsigned long>(Result))};
    }
}

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::filesystem::path& Path, const char* EntryPoint, const char* Profile) {
    Microsoft::WRL::ComPtr<ID3DBlob> Code{};
    Microsoft::WRL::ComPtr<ID3DBlob> Errors{};
    const HRESULT Result{D3DCompileFromFile(Path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint, Profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &Code, &Errors)};
    if (FAILED(Result) && Errors) {
        std::cerr << static_cast<const char*>(Errors->GetBufferPointer());
    }
    CheckResult(Result);
    return Code;
}

DXGI_FORMAT GetVertexFormat(const std::string& Name) {
    if (Name == "Uint") {
        return DXGI_FORMAT_R32_UINT;
    }
    if (Name == "Float2") {
        return DXGI_FORMAT_R32G32_FLOAT;
    }
    if (Name == "Float3") {
        return DXGI_FORMAT_R32G32B32_FLOAT;
    }
    if (Name == "Float4") {
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    throw std::runtime_error{"Unknown vertex format: " + Name};
}

Microsoft::WRL::ComPtr<ID3D11InputLayout> ValidatePipeline(ID3D11Device* Device, const std::filesystem::path& Path) {
    std::ifstream File{Path};
    const std::string Text{std::istreambuf_iterator<char>{File}, std::istreambuf_iterator<char>{}};
    rapidjson::Document Document{};
    Document.Parse(Text.c_str());
    if (Document.HasParseError()) {
        throw std::runtime_error{"Cannot parse pipeline: " + Path.string()};
    }
    const rapidjson::Value& VertexShader{Document["VertexShader"]};
    const Microsoft::WRL::ComPtr<ID3DBlob> Code{CompileShader(VertexShader["Source"].GetString(), VertexShader["EntryPoint"].GetString(), VertexShader["Profile"].GetString())};
    for (const char* Stage : {"PixelShader", "GeometryShader"}) {
        if (Document.HasMember(Stage)) {
            const rapidjson::Value& Shader{Document[Stage]};
            CompileShader(Shader["Source"].GetString(), Shader["EntryPoint"].GetString(), Shader["Profile"].GetString());
        }
    }
    std::vector<D3D11_INPUT_ELEMENT_DESC> Elements{};
    for (const rapidjson::Value& Input : Document["InputLayout"].GetArray()) {
        D3D11_INPUT_ELEMENT_DESC Element{};
        Element.SemanticName = Input["SemanticName"].GetString();
        Element.SemanticIndex = Input["SemanticIndex"].GetUint();
        Element.Format = GetVertexFormat(Input["Format"].GetString());
        Element.InputSlot = Input["InputSlot"].GetUint();
        Element.AlignedByteOffset = Input["AlignedByteOffset"].GetUint();
        Element.InputSlotClass = std::string{Input["InputClassification"].GetString()} == "PerInstance" ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        Element.InstanceDataStepRate = Input["InstanceDataStepRate"].GetUint();
        Elements.push_back(Element);
    }
    Microsoft::WRL::ComPtr<ID3D11InputLayout> Layout{};
    if (!Elements.empty()) {
        CheckResult(Device->CreateInputLayout(Elements.data(), static_cast<UINT>(Elements.size()), Code->GetBufferPointer(), Code->GetBufferSize(), &Layout));
    }
    return Layout;
}

Microsoft::WRL::ComPtr<ID3D11Buffer> CreateTestBuffer(ID3D11Device* Device, UINT ByteSize, UINT BindFlags, const void* Data = nullptr, UINT StructureStride = 0) {
    D3D11_BUFFER_DESC Description{};
    Description.ByteWidth = ByteSize;
    Description.Usage = Data != nullptr ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    Description.BindFlags = BindFlags;
    Description.StructureByteStride = StructureStride;
    Description.MiscFlags = StructureStride != 0 ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : 0;
    const D3D11_SUBRESOURCE_DATA InitialData{Data};
    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer{};
    CheckResult(Device->CreateBuffer(&Description, Data != nullptr ? &InitialData : nullptr, &Buffer));
    return Buffer;
}

void TestModelSelection(ID3D11Device* Device, ID3D11DeviceContext* Context) {
    static_assert(sizeof(FTestModelContext) == 72);
    static_assert(sizeof(FTestVertexOutput) == 20);
    constexpr UINT ModelCount{50000};
    constexpr UINT VertexCount{3};
    std::vector<UINT> ModelIndices{};
    std::vector<FTestModelContext> Models{};
    ModelIndices.resize(ModelCount);
    Models.resize(ModelCount);
    for (UINT Index{}; Index < ModelCount; ++Index) {
        ModelIndices[Index] = Index;
        Models[Index].mWorld[0] = 1.0f;
        Models[Index].mWorld[5] = 1.0f;
        Models[Index].mWorld[10] = 1.0f;
        Models[Index].mWorld[15] = 1.0f;
        Models[Index].mWorld[12] = static_cast<float>(Index);
        Models[Index].mMaterialIndex = Index;
    }
    const Microsoft::WRL::ComPtr<ID3D11Buffer> ModelIndexBuffer{CreateTestBuffer(Device, ModelCount * sizeof(UINT), D3D11_BIND_VERTEX_BUFFER, ModelIndices.data())};
    const Microsoft::WRL::ComPtr<ID3D11Buffer> ModelBuffer{CreateTestBuffer(Device, ModelCount * sizeof(FTestModelContext), D3D11_BIND_SHADER_RESOURCE, Models.data(), sizeof(FTestModelContext))};
    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDescription{};
    ViewDescription.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    ViewDescription.Buffer.NumElements = ModelCount;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelView{};
    CheckResult(Device->CreateShaderResourceView(ModelBuffer.Get(), &ViewDescription, &ModelView));
    Context->VSSetShaderResources(0, 1, ModelView.GetAddressOf());
    std::array<float, 76> ViewConstants{};
    for (UINT Matrix{}; Matrix < 4; ++Matrix) {
        for (UINT Axis{}; Axis < 4; ++Axis) {
            ViewConstants[Matrix * 16 + Axis * 5] = 1.0f;
        }
    }
    const Microsoft::WRL::ComPtr<ID3D11Buffer> ViewBuffer{CreateTestBuffer(Device, sizeof(ViewConstants), D3D11_BIND_CONSTANT_BUFFER, ViewConstants.data())};
    Context->VSSetConstantBuffers(1, 1, ViewBuffer.GetAddressOf());
    const std::array<float, VertexCount * 4> Vertices{};
    const std::array<UINT, VertexCount> Indices{2, 0, 1};
    const Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer{CreateTestBuffer(Device, sizeof(Vertices), D3D11_BIND_VERTEX_BUFFER, Vertices.data())};
    const Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer{CreateTestBuffer(Device, sizeof(Indices), D3D11_BIND_INDEX_BUFFER, Indices.data())};
    ID3D11Buffer* Buffers[]{VertexBuffer.Get(), VertexBuffer.Get(), VertexBuffer.Get(), VertexBuffer.Get()};
    const UINT Strides[]{12, 12, 8, 16};
    const UINT Offsets[]{0, 0, 0, 0};
    Context->IASetVertexBuffers(0, 4, Buffers, Strides, Offsets);
    Context->IASetIndexBuffer(IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    const Microsoft::WRL::ComPtr<ID3D11InputLayout> Layout{ValidatePipeline(Device, "Content/Pipeline/Base/Base_Unlit.json")};
    Context->IASetInputLayout(Layout.Get());
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    const Microsoft::WRL::ComPtr<ID3DBlob> Code{CompileShader("Content/Shader/Base_Unlit.hlsl", "mainVS", "vs_5_0")};
    Microsoft::WRL::ComPtr<ID3D11VertexShader> Shader{};
    CheckResult(Device->CreateVertexShader(Code->GetBufferPointer(), Code->GetBufferSize(), nullptr, &Shader));
    Context->VSSetShader(Shader.Get(), nullptr, 0);
    const D3D11_SO_DECLARATION_ENTRY Entries[]{{0, "SV_POSITION", 0, 0, 4, 0}, {0, "Jungle", 1, 0, 1, 0}};
    const UINT OutputStride{sizeof(FTestVertexOutput)};
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> Capture{};
    CheckResult(Device->CreateGeometryShaderWithStreamOutput(Code->GetBufferPointer(), Code->GetBufferSize(), Entries, 2, &OutputStride, 1, D3D11_SO_NO_RASTERIZED_STREAM, nullptr, &Capture));
    Context->GSSetShader(Capture.Get(), nullptr, 0);
    const Microsoft::WRL::ComPtr<ID3D11Buffer> OutputBuffer{CreateTestBuffer(Device, ModelCount * VertexCount * sizeof(FTestVertexOutput), D3D11_BIND_STREAM_OUTPUT)};
    D3D11_BUFFER_DESC ReadDescription{};
    ReadDescription.ByteWidth = ModelCount * VertexCount * sizeof(FTestVertexOutput);
    ReadDescription.Usage = D3D11_USAGE_STAGING;
    ReadDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Buffer> Readback{};
    CheckResult(Device->CreateBuffer(&ReadDescription, nullptr, &Readback));
    const FTestDraw Draws[]{{0, ModelCount, true}, {ModelCount - 1, 1, false}, {100, 3, true}, {32768, 1, false}, {ModelCount - 3, 3, true}, {7, 1, false}, {0, 1, false}, {1, 2, true}};
    for (const FTestDraw& Draw : Draws) {
        const UINT ModelStride{sizeof(UINT)};
        const UINT ModelOffset{Draw.mInstanced ? 0u : Draw.mModelIndex * ModelStride};
        Context->IASetVertexBuffers(4, 1, ModelIndexBuffer.GetAddressOf(), &ModelStride, &ModelOffset);
        const UINT OutputOffset{};
        Context->SOSetTargets(1, OutputBuffer.GetAddressOf(), &OutputOffset);
        if (Draw.mInstanced) {
            Context->DrawIndexedInstanced(VertexCount, Draw.mInstanceCount, 0, 0, Draw.mModelIndex);
        } else {
            Context->DrawIndexed(VertexCount, 0, 0);
        }
        Context->SOSetTargets(0, nullptr, nullptr);
        Context->CopyResource(Readback.Get(), OutputBuffer.Get());
        D3D11_MAPPED_SUBRESOURCE Mapped{};
        CheckResult(Context->Map(Readback.Get(), 0, D3D11_MAP_READ, 0, &Mapped));
        const FTestVertexOutput* Results{static_cast<const FTestVertexOutput*>(Mapped.pData)};
        bool Valid{true};
        for (UINT Instance{}; Instance < Draw.mInstanceCount; ++Instance) {
            const UINT Expected{Draw.mModelIndex + Instance};
            for (UINT Vertex{}; Vertex < VertexCount; ++Vertex) {
                const FTestVertexOutput& Result{Results[Instance * VertexCount + Vertex]};
                Valid = Valid && Result.mMaterialIndex == Expected && Result.mPosition[0] == static_cast<float>(Expected);
            }
        }
        Context->Unmap(Readback.Get(), 0);
        if (!Valid) {
            throw std::runtime_error{"Incorrect model selection at " + std::to_string(Draw.mModelIndex)};
        }
        std::cout << "PASS " << (Draw.mInstanced ? "DrawIndexedInstanced" : "DrawIndexed") << " start=" << Draw.mModelIndex << " count=" << Draw.mInstanceCount << '\n';
    }
}

int main() {
    try {
        Microsoft::WRL::ComPtr<ID3D11Device> Device{};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context{};
        CheckResult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &Context));
        UINT PipelineCount{};
        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator{"Content/Pipeline"}) {
            if (Entry.path().extension() == ".json") {
                ValidatePipeline(Device.Get(), Entry.path());
                ++PipelineCount;
            }
        }
        std::cout << "PASS shader compilation and input layouts: " << PipelineCount << " pipelines\n";
        TestModelSelection(Device.Get(), Context.Get());
    } catch (const std::exception& Error) {
        std::cerr << Error.what() << '\n';
        return 1;
    }
    return 0;
}
