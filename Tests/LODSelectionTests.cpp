#include "pch.h"
#include "Render/FLODSelection.h"
#include "Render/FRenderQueue.h"
#include "Render/FSceneRenderSurface.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMesh.h"
#include "Asset/UMaterial.h"
#include "Asset/FMaterialBuffer.h"
#include <iostream>
#include <stdexcept>
#include <d3dcompiler.h>

void Check(bool OK, const char* Message) { if (!OK) { throw std::runtime_error(Message); } }

class FQueueTestMaterial final : public UMaterial {
public:
    void BuildGPUData(FMaterialGPUSlot& Out) const override { Out = {}; }
};

class FQueueTestAssets final : public IAssetRegistry {
public:
    UMesh Mesh;
    FQueueTestMaterial Material;
    UPipeline Pipeline;
    const UObject* ResolveAssetObject(FAssetHandle H) const override {
        if (H.mId == 0) { return &Mesh; } if (H.mId == 1) { return &Material; } if (H.mId == 2) { return &Pipeline; } return nullptr;
    }
    FAssetHandle FindAsset(const FAssetPath&) const override { return {}; }
    FAssetHandle FindAsset(const FGuid&) const override { return {}; }
    const FAssetPath* GetAssetPath(FAssetHandle) const override { return nullptr; }
    const FGuid* GetAssetGuid(FAssetHandle) const override { return nullptr; }
    TArray<FAssetHandle> GetAssetHandles(const FTypeInfo&) const override { return {}; }
    FAssetHandle EnsureDefaultStaticMeshMaterial() const override { return {}; }
    FAssetHandle EnsureDefaultStaticMeshPipeline() const override { return {}; }
};

void CheckQueue(ID3D11Device* Device) {
    FQueueTestAssets Assets;
    FMaterialBuffer Materials; Check(Materials.Initialize(Device, 4) && Materials.RegisterMaterial(&Assets.Material), "queue material");
    TArray<FVector3> Positions{{0,0,0},{1,0,0},{0,1,0}}; TArray<Uint32> Indices{0,1,2};
    Check(Assets.Mesh.Make(Device, std::span<const Uint32>{Indices}, MakeVertexAttribute<EVertexAttribute::Position>(Positions)), "queue mesh");
    FActorProbe Probe; Probe.mWorld.SetIdentity(); Probe.mMeshHandle = {0,1}; Probe.mMaterialHandle = {1,1}; Probe.mPipelineHandle = {2,1}; Probe.mOwnerHandle = {5,1};
    Probe.mWorldSphereBounds = {{0,0,0.5f}, 0.001f}; Probe.mWorldAABB = {{0,0,0.5f}, {0.0005f,0.0005f,0.0005f}};
    DirectX::BoundingOrientedBox::CreateFromBoundingBox(Probe.mWorldOBB, Probe.mWorldAABB);
    FSceneRenderData Data; Data.mSceneId = AllocateRenderSceneId(); Data.mRevision = 1; Data.mObjectUpdates.push_back({{0,1}, Probe, false});
    FRenderScene Scene{Data.mSceneId}; Scene.Synchronize(&Assets, Data);
    FSceneRenderSurface Surface; Surface.InitializeOffscreen(Device, 64, 720);
    FRenderView View; View.mTarget = &Surface; View.mPasses.reset(); View.SetPassEnabled(ERenderPass::SceneGeometry, true);
    View.mCamera.mView.SetIdentity(); View.mCamera.mProjection.SetIdentity(); View.mCamera.mViewProjection.SetIdentity();
    FRenderQueue Queue; Queue.Build(&Assets, Scene, View); Check(Queue.GetDrawRecords().empty(), "subpixel object not CPU culled");
    // Only the viewport height changes; the camera, scene and template revisions stay identical.
    Check(Surface.Resize(Device, 64, 2160), "resize"); Queue.Build(&Assets, Scene, View);
    Check(Queue.GetDrawRecords().size() == 1 && Queue.GetDrawRecords()[0].mLODDither == 0, "resize reused stale culled queue");
    Check(Surface.Resize(Device, 64, 1080), "resize fade"); Queue.Build(&Assets, Scene, View);
    Check(Queue.GetDrawRecords().size() == 1 && Queue.GetDrawRecords()[0].mLODDither > 0, "fade band did not reach draw record");
    Check(Surface.Resize(Device, 64, 720), "resize selected"); View.mSelectedActorHandle = Probe.mOwnerHandle; Queue.Build(&Assets, Scene, View);
    Check(Queue.GetDrawRecords().size() == 1 && Queue.GetDrawRecords()[0].mLODDither == 0, "selected object disappeared");
    View.mSelectedActorHandle = {}; View.mUseLOD = false; Queue.Build(&Assets, Scene, View);
    Check(Queue.GetDrawRecords().size() == 1 && Queue.GetDrawRecords()[0].mLODDither == 0, "LOD disabled still culled object");
    Check(Assets.Mesh.GenerateLOD(Device, 1, 0.5f), "queue LOD");
    Probe.mWorldSphereBounds.Radius = GLODSettings[0].mScreenSize;
    ++Data.mRevision; Data.mObjectUpdates[0].mProbe = Probe; Scene.Synchronize(&Assets, Data);
    View.mUseLOD = true; Queue.Build(&Assets, Scene, View);
    Check(Queue.GetDrawRecords().size() == 2 && Queue.GetItems(ERenderPass::SceneGeometry).size() == 2, "transition needs exactly two LOD batches");
    const auto& Records{Queue.GetDrawRecords()};
    Check(std::abs(Records[0].mLODDither - 0.5f) < 1e-5f && Records[0].mLODDither == -Records[1].mLODDither, "complementary draw record weights");
    Check(Records[0].mObjectIndex == Records[1].mObjectIndex && Records[0].mMaterialIndex == Records[1].mMaterialIndex, "transition object/material mismatch");
    Materials.Reset();
    std::cout << "Production queue: subpixel culling, fade records, viewport cache invalidation, selection and LOD opt-out: PASS\n";
}

void CheckGPU() {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> Device; ComPtr<ID3D11DeviceContext> Context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &Context)), "WARP device");
    CheckQueue(Device.Get());
    auto Compile = [](const wchar_t* File, const char* Entry, const char* Profile) {
        ComPtr<ID3DBlob> Code, Errors;
        const HRESULT HR{D3DCompileFromFile(File, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, Entry, Profile, 0, 0, &Code, &Errors)};
        if (FAILED(HR) && Errors) { std::cerr << static_cast<const char*>(Errors->GetBufferPointer()); }
        Check(SUCCEEDED(HR), "shader compile"); return Code;
    };
    for (const wchar_t* File : {L"Content/Shader/Base.hlsl", L"Content/Shader/Base_Unlit.hlsl", L"Content/Shader/TexturedBase.hlsl", L"Content/Shader/TexturedBase_Unlit.hlsl", L"Content/Shader/Alternate.hlsl", L"Content/Shader/Alternate_Unlit.hlsl", L"Content/Shader/VAT.hlsl", L"Content/Shader/SkyDome.hlsl"}) {
        Compile(File, "mainVS", "vs_5_0"); Compile(File, "mainPS", "ps_5_0");
    }
    Compile(L"Content/Shader/Outline.hlsl", "MainVS", "vs_5_0");
    Compile(L"Content/Shader/Outline.hlsl", "MainPS", "ps_5_0");
    Compile(L"Content/Shader/Outline.hlsl", "MainGS", "gs_5_0");
    const auto VSCode{Compile(L"Tests/LODDitherTest.hlsl", "MainVS", "vs_5_0")};
    const auto PSCode{Compile(L"Tests/LODDitherTest.hlsl", "MainPS", "ps_5_0")};
    ComPtr<ID3D11VertexShader> VS; ComPtr<ID3D11PixelShader> PS;
    Check(SUCCEEDED(Device->CreateVertexShader(VSCode->GetBufferPointer(), VSCode->GetBufferSize(), nullptr, &VS)), "VS");
    Check(SUCCEEDED(Device->CreatePixelShader(PSCode->GetBufferPointer(), PSCode->GetBufferSize(), nullptr, &PS)), "PS");
    Context->VSSetShader(VS.Get(), nullptr, 0); Context->PSSetShader(PS.Get(), nullptr, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11_RASTERIZER_DESC Raster{}; Raster.FillMode = D3D11_FILL_SOLID; Raster.CullMode = D3D11_CULL_NONE; Raster.DepthClipEnable = true;
    ComPtr<ID3D11RasterizerState> RS; Check(SUCCEEDED(Device->CreateRasterizerState(&Raster, &RS)), "rasterizer"); Context->RSSetState(RS.Get());
    FSceneRenderSurface Surface; Surface.InitializeOffscreen(Device.Get(), 64, 64); Surface.Bind(Context.Get());
    D3D11_DEPTH_STENCIL_DESC Depth{}; ComPtr<ID3D11DepthStencilState> DS;
    Check(SUCCEEDED(Device->CreateDepthStencilState(&Depth, &DS)), "depth state"); Context->OMSetDepthStencilState(DS.Get(), 0);
    D3D11_BUFFER_DESC Buffer{}; Buffer.ByteWidth = 16; Buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> Constants; Check(SUCCEEDED(Device->CreateBuffer(&Buffer, nullptr, &Constants)), "constants");
    Context->PSSetConstantBuffers(0, 1, Constants.GetAddressOf());
    auto Draw = [&](float Dither, bool Incoming) {
        const float Values[]{Dither, Incoming ? 0.0f : 1.0f, Incoming ? 1.0f : 0.0f, 0.0f};
        Context->UpdateSubresource(Constants.Get(), 0, nullptr, Values, 0, 0); Context->Draw(3, 0);
    };
    ComPtr<ID3D11Resource> Resource; Surface.GetShaderResourceView()->GetResource(&Resource);
    ComPtr<ID3D11Texture2D> Texture; Resource.As(&Texture); D3D11_TEXTURE2D_DESC Desc{}; Texture->GetDesc(&Desc);
    Desc.Usage = D3D11_USAGE_STAGING; Desc.BindFlags = 0; Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; Desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> Readback; Check(SUCCEEDED(Device->CreateTexture2D(&Desc, nullptr, &Readback)), "readback");
    const float Black[]{0,0,0,1};
    for (float Blend : {0.001f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f}) {
        Surface.Clear(Context.Get(), Black); Draw(Blend, false); Draw(-Blend, true);
        Context->CopyResource(Readback.Get(), Texture.Get()); D3D11_MAPPED_SUBRESOURCE Mapped{};
        Check(SUCCEEDED(Context->Map(Readback.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "map");
        Uint32 Green{};
        for (Uint32 Y{}; Y < 64; ++Y) {
            const auto* Row{static_cast<const Uint8*>(Mapped.pData) + Y*Mapped.RowPitch};
            for (Uint32 X{}; X < 64; ++X) {
                Check(Row[X*4] + Row[X*4+1] == 255, "GPU dither produced uncovered pixels");
                Green += Row[X*4+1] == 255;
            }
        }
        Context->Unmap(Readback.Get(), 0);
        Check(std::abs(float(Green)/4096 - Blend) < 0.015f, "GPU blend coverage");
    }
    Surface.Clear(Context.Get(), Black); Draw(1, false); Context->CopyResource(Readback.Get(), Texture.Get());
    D3D11_MAPPED_SUBRESOURCE Mapped{}; Check(SUCCEEDED(Context->Map(Readback.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "fade map");
    for (Uint32 Y{}; Y < 64; ++Y) { for (Uint32 X{}; X < 64; ++X) {
        Check(static_cast<const Uint8*>(Mapped.pData)[Y*Mapped.RowPitch + X*4] == 0, "cull endpoint still visible");
    } }
    Context->Unmap(Readback.Get(), 0);
    std::cout << "All mesh shaders + GPU complementary coverage + fully faded endpoint: PASS\n";
}

int main() {
    try {
        constexpr Uint32 Mask{(1u << GLODCount) - 1};
        for (Uint32 Level{}; Level + 1 < GLODCount; ++Level) {
            const float Boundary{GLODSettings[Level].mScreenSize};
            const auto Middle{SelectMeshLOD(Boundary, 1080, Mask, true)};
            Check(Middle.mLevel == Level && Middle.mNextLevel == Level + 1 && std::abs(Middle.mDither - 0.5f) < 1e-5f, "midpoint blend");
            const auto Above{SelectMeshLOD(Boundary * (1 + GLODTransitionHalfWidth + 0.001f), 1080, Mask, true)};
            const auto Below{SelectMeshLOD(Boundary * (1 - GLODTransitionHalfWidth - 0.001f), 1080, Mask, true)};
            Check(Above.mLevel == Level && Above.mNextLevel == UINT32_MAX && Above.mDither == 0, "upper endpoint");
            Check(Below.mLevel == Level + 1 && Below.mNextLevel == UINT32_MAX && Below.mDither == 0, "lower endpoint");
            float Previous{-1};
            for (int I{1}; I < 100; ++I) {
                const float Size{Boundary * (1 + GLODTransitionHalfWidth - 2*GLODTransitionHalfWidth*I/100)};
                const auto Selection{SelectMeshLOD(Size, 1080, Mask, true)};
                Check(Selection.mDither >= Previous && Selection.mDither <= 1, "continuous monotonic blend");
                // The two passes partition every pixel, including exact equality.
                for (int Pixel{}; Pixel < 128; ++Pixel) {
                    const float Noise{Pixel / 128.0f};
                    Check(int(Noise >= Selection.mDither) + int(Noise < Selection.mDither) == 1, "complementary coverage");
                }
                Previous = Selection.mDither;
            }
        }
        for (float Height : {720.0f, 1080.0f, 2160.0f}) {
            Check(SelectMeshLOD(GLODCullPixels / Height, Height, Mask, true).mCulled, "pixel culling");
            const auto Fade{SelectMeshLOD((GLODFadePixels + GLODCullPixels)*0.5f / Height, Height, Mask, true)};
            Check(!Fade.mCulled && Fade.mNextLevel == UINT32_MAX && std::abs(Fade.mDither - 0.5f) < 1e-5f, "cull fade");
            Check(!SelectMeshLOD(0.1f/Height, Height, Mask, false).mCulled, "selected object cull exemption");
        }
        for (Uint32 Available{1}; Available <= Mask; Available += 2) {
            for (float Size{0.0001f}; Size < 2; Size *= 1.01f) {
                const auto Selection{SelectMeshLOD(Size, 1080, Available, true)};
                Check((Available & (1u << Selection.mLevel)) != 0, "unavailable primary LOD");
                Check(Selection.mNextLevel == UINT32_MAX || (Available & (1u << Selection.mNextLevel)) != 0, "unavailable secondary LOD");
            }
        }
        Check(SelectMeshLOD(0, 1080, Mask, true).mLevel == 0, "invalid bounds fallback");
        Check(!SelectMeshLOD(0.0001f, 0, Mask, true).mCulled, "missing viewport fallback");
        static_assert(sizeof(FMeshDrawRecord) == 16);
        CheckGPU();
        std::cout << "LOD boundaries, continuous blend, complementary coverage, pixel culling, selected objects, missing levels: PASS\n";
    } catch (const std::exception& E) { std::cerr << E.what() << '\n'; return 1; }
}
