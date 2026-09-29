#include "pch.h"
#include "FShader.h"

#include "Core/Base/ErrorHandler.h"

#include <d3dcompiler.h>

#include <cstring>

using Microsoft::WRL::ComPtr;

bool FShader::Initialize(ID3D11Device* Device, const FShaderDescription& Description) {
    if (Device == nullptr) {
        ErrorHandler::Report("Shader::Initialize", "A valid Direct3D device is required to initialize a shader.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    FShader::Reset();

    UINT CompileFlags{ D3DCOMPILE_OPTIMIZATION_LEVEL3 };

#ifdef _DEBUG
    CompileFlags |= D3DCOMPILE_DEBUG;
    CompileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> ShaderBlob{};
    ComPtr<ID3DBlob> ErrorBlob{};

    HRESULT Result{D3DCompileFromFile(Description.mSource.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, Description.mEntryPoint.c_str(), Description.mProfile.c_str(), CompileFlags, 0, ShaderBlob.GetAddressOf(), ErrorBlob.GetAddressOf())};

    if (FAILED(Result)) {
        if (ErrorBlob) {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
        }

        ErrorHandler::ReportHRESULT(Result, "Shader::Initialize", "Shader compilation failed.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    mStage = Description.mStage;

    mByteCode.resize(ShaderBlob->GetBufferSize());
    std::memcpy(mByteCode.data(), ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize());

    switch (mStage) {
        case EShaderStage::Vertex:
            Result = Device->CreateVertexShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, mVertexShader.GetAddressOf());
            break;

        case EShaderStage::Pixel:
            Result = Device->CreatePixelShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, mPixelShader.GetAddressOf());
            break;

        case EShaderStage::Geometry:
            Result = Device->CreateGeometryShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, mGeometryShader.GetAddressOf());
            break;

        case EShaderStage::Hull:
            Result = Device->CreateHullShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, mHullShader.GetAddressOf());
            break;

        case EShaderStage::Domain:
            Result = Device->CreateDomainShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, mDomainShader.GetAddressOf());
            break;

        case EShaderStage::Compute:
            Result = Device->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, mComputeShader.GetAddressOf());
            break;

        default:
            ErrorHandler::Report("Shader::Initialize", "The shader description contains an invalid shader stage.", ErrorHandler::EErrorLevel::Error);
            return false;
    }

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Shader::Initialize", "Failed to create the Direct3D shader.", ErrorHandler::EErrorLevel::Error);
        FShader::Reset();
        return false;
    }

    return true;
}

void FShader::Reset() {
    mByteCode.clear();

    mVertexShader.Reset();
    mPixelShader.Reset();
    mGeometryShader.Reset();
    mHullShader.Reset();
    mDomainShader.Reset();
    mComputeShader.Reset();
}

EShaderStage FShader::GetStage() const noexcept {
    return mStage;
}

const void* FShader::GetByteCodeData() const noexcept {
    return mByteCode.data();
}

std::size_t FShader::GetByteCodeSize() const noexcept {
    return mByteCode.size();
}

ID3D11VertexShader* FShader::GetVertexShader() const noexcept {
    return mVertexShader.Get();
}

ID3D11PixelShader* FShader::GetPixelShader() const noexcept {
    return mPixelShader.Get();
}

ID3D11GeometryShader* FShader::GetGeometryShader() const noexcept {
    return mGeometryShader.Get();
}

ID3D11HullShader* FShader::GetHullShader() const noexcept {
    return mHullShader.Get();
}

ID3D11DomainShader* FShader::GetDomainShader() const noexcept {
    return mDomainShader.Get();
}

ID3D11ComputeShader* FShader::GetComputeShader() const noexcept {
    return mComputeShader.Get();
}
