#include "pch.h"
#include "Asset/UMesh.h"
#include "Asset/USurfaceOpaque.h"
#include "Asset/UTexture.h"
#include "Asset/FMaterialBuffer.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Core/Base/FTransform.h"
#include "Render/FMeshRenderer.h"
#include "Render/FFrameResource.h"
#include "Render/FSceneRenderSurface.h"
#include "World/Component/UCameraComponent.h"
#include <rapidjson/istreamwrapper.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <DirectXTex.h>
#undef GetObject

using Microsoft::WRL::ComPtr;
void Require(bool OK, const char* Message) { if (!OK) { throw std::runtime_error(Message); } }

// Use the production queue, shaders, buffers and draw path, without editor/UI overhead.
class FBenchmarkAssets final : public IAssetRegistry {
public:
    std::vector<std::unique_ptr<UObject>> Assets;
    FAssetHandle Add(std::unique_ptr<UObject> Asset) {
        FAssetHandle Handle{static_cast<Uint32>(Assets.size()), 1};
        Assets.push_back(std::move(Asset)); return Handle;
    }
    FAssetHandle FindAsset(const FAssetPath&) const override { return {}; }
    FAssetHandle FindAsset(const FGuid&) const override { return {}; }
    const FAssetPath* GetAssetPath(FAssetHandle) const override { return nullptr; }
    const FGuid* GetAssetGuid(FAssetHandle) const override { return nullptr; }
    TArray<FAssetHandle> GetAssetHandles(const FTypeInfo&) const override { return {}; }
    FAssetHandle EnsureDefaultStaticMeshMaterial() const override { return {}; }
    FAssetHandle EnsureDefaultStaticMeshPipeline() const override { return {}; }
    const UObject* ResolveAssetObject(FAssetHandle Handle) const override {
        return Handle.mId < Assets.size() ? Assets[Handle.mId].get() : nullptr;
    }
};

FVector3 Vector(const rapidjson::Value& V) { return {V[0].GetFloat(), V[1].GetFloat(), V[2].GetFloat()}; }
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point Start) { return std::chrono::duration<double, std::milli>(Clock::now() - Start).count(); }

int main(int argc, char** argv) {
    try {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const std::string Label{argc > 1 ? argv[1] : "current"};
        const std::filesystem::path OutputDirectory{"Build/LODResults"};
        std::filesystem::create_directories(OutputDirectory);
        const UINT Height{argc > 2 ? static_cast<UINT>(std::stoi(argv[2])) : 1080u};
        const UINT Width{Height * 16 / 9};
        ComPtr<ID3D11Device> Device;
        ComPtr<ID3D11DeviceContext> Context;
        Require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &Context)), "hardware device");
        ComPtr<IDXGIDevice> DXGI; Device.As(&DXGI);
        ComPtr<IDXGIAdapter> Adapter; DXGI->GetAdapter(&Adapter);
        DXGI_ADAPTER_DESC AdapterInfo{}; Adapter->GetDesc(&AdapterInfo);
        std::wcout << L"GPU: " << AdapterInfo.Description << std::endl;
        FBenchmarkAssets Registry;
        FMaterialBuffer Materials;
        Require(Materials.Initialize(Device.Get(), 64), "materials");
        auto Pipeline{std::make_unique<UPipeline>()};
        Require(Pipeline->Initialize(Device.Get(), "Content/Pipeline/TexturedBase"), "pipeline");
        const FAssetHandle PipelineHandle{Registry.Add(std::move(Pipeline))};
        std::map<std::string, std::pair<FAssetHandle, FAssetHandle>> Meshes;
        for (const std::string Name : {"apple_mid", "bitten_apple_mid"}) {
            auto Material{std::make_unique<USurfaceOpaque>()};
            Require(Material->Initialize(Device.Get(), "Content/Data/" + Name + ".mtl", [&](const std::filesystem::path& Path) {
                auto Texture{std::make_unique<UTexture>()};
                Require(Texture->Initialize(Device.Get(), Path, false), "texture");
                return Registry.Add(std::move(Texture));
            }), "material");
            Require(Materials.RegisterMaterial(Material.get()), "material slot");
            const auto MaterialHandle{Registry.Add(std::move(Material))};
            auto Mesh{std::make_unique<UMesh>()};
            Require(Mesh->Initialize(Device.Get(), "Content/Data/" + Name + ".obj", "Content/Data/" + Name + ".bin",
                [MaterialHandle](const auto&) { return MaterialHandle; },
                [&](FAssetHandle H, const FString& Group) { return Registry.ResolveAsset<UMaterial>(H)->FindGroupIndex(Group); }, false), "mesh");
            for (Uint32 Level{1}; Level < GLODCount; ++Level) {
                Require(Mesh->GenerateLOD(Device.Get(), Level, GLODSettings[Level].mTargetRatio), "LOD generation");
                std::cout << Name << " LOD" << Level << " triangles=" << Mesh->GetIndexCount(Level)/3 << '\n';
            }
            Meshes.emplace("Data/" + Name + ".obj", std::make_pair(Registry.Add(std::move(Mesh)), MaterialHandle));
        }
        Materials.Flush(Context.Get());
        std::ifstream Input{"scenes/Default.scene"};
        rapidjson::IStreamWrapper Stream{Input}; rapidjson::Document Document; Document.ParseStream(Stream);
        Require(!Document.HasParseError(), "scene JSON");
        FSceneRenderData Data; Data.mSceneId = AllocateRenderSceneId(); Data.mRevision = 1;
        Uint32 Index{};
        for (const auto& Entry : Document["Primitives"].GetObject()) {
            const auto& P{Entry.value};
            const auto [MeshHandle, MaterialHandle]{Meshes.at(P["ObjStaticMeshAsset"].GetString())};
            const UMesh* Mesh{Registry.ResolveAsset<UMesh>(MeshHandle)};
            FActorProbe Probe;
            Probe.mWorld = FTransform{Vector(P["Location"]), FRotator{Vector(P["Rotation"])}, Vector(P["Scale"])}.ToMatrixWithScale();
            Probe.mMeshHandle = MeshHandle; Probe.mMaterialHandle = MaterialHandle; Probe.mPipelineHandle = PipelineHandle;
            Probe.mOwnerHandle = {Index, 1};
            auto Bounds{Mesh->GetBoundingBox()};
            DirectX::BoundingSphere Sphere{Bounds.Center, std::sqrt(Bounds.Extents.x*Bounds.Extents.x + Bounds.Extents.y*Bounds.Extents.y + Bounds.Extents.z*Bounds.Extents.z)};
            Bounds.Transform(Probe.mWorldOBB, Probe.mWorld.ToSimpleMath());
            Sphere.Transform(Probe.mWorldSphereBounds, Probe.mWorld.ToSimpleMath());
            DirectX::XMFLOAT3 Corners[8]; Probe.mWorldOBB.GetCorners(Corners);
            DirectX::BoundingBox::CreateFromPoints(Probe.mWorldAABB, 8, Corners, sizeof(Corners[0]));
            Data.mObjectUpdates.push_back({{Index++, 1}, Probe, false});
        }
        Data.mLightProbes.push_back(FLightProbe{});
        Data.mLightProbes.back().mDirection = {0.4f, 0.3f, -1.0f};
        FRenderScene Scene{Data.mSceneId}; Scene.Synchronize(&Registry, Data);
        FSceneRenderSurface Surface; Surface.InitializeOffscreen(Device.Get(), Width, Height);
        FFrameResource Frame; Require(Frame.Initialize(Device.Get(), Context.Get()), "frame");
        FRenderQueue Queue; FMeshRenderer Renderer;
        FRenderView View; View.mTarget = &Surface; View.mPasses.reset(); View.SetPassEnabled(ERenderPass::SceneGeometry, true);
        D3D11_SAMPLER_DESC SD{}; SD.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SD.AddressU = SD.AddressV = SD.AddressW = D3D11_TEXTURE_ADDRESS_WRAP; SD.MaxLOD = D3D11_FLOAT32_MAX;
        ComPtr<ID3D11SamplerState> Sampler; Require(SUCCEEDED(Device->CreateSamplerState(&SD, &Sampler)), "sampler");
        Context->PSSetSamplers(0, 1, Sampler.GetAddressOf());
        Context->VSSetSamplers(0, 1, Sampler.GetAddressOf());
        auto Query = [&](D3D11_QUERY Kind) { ComPtr<ID3D11Query> Q; D3D11_QUERY_DESC D{Kind, 0}; Require(SUCCEEDED(Device->CreateQuery(&D, &Q)), "query"); return Q; };
        auto Done{Query(D3D11_QUERY_EVENT)}, Disjoint{Query(D3D11_QUERY_TIMESTAMP_DISJOINT)}, StartGPU{Query(D3D11_QUERY_TIMESTAMP)}, EndGPU{Query(D3D11_QUERY_TIMESTAMP)};
        const auto& C{Document["PerspectiveCamera"]};
        const FVector3 Origin{Vector(C["Location"])};
        const FRotator Rotation{Vector(C["Rotation"])};
        std::ofstream CSV{OutputDirectory / ("LOD-" + Label + ".csv")};
        CSV << "path,width,height,objects,queue_ms,cpu_submit_ms,gpu_ms,serial_frame_ms,render_only_fps,triangles,draw_calls,records\n";
        for (int Path{}; Path < 3; ++Path) {
            std::vector<double> CPU, GPU, Total, Prepare; double Triangles{}, Draws{}, Records{};
            for (int FrameIndex{}; FrameIndex < 100; ++FrameIndex) {
                const float T{float(FrameIndex % 80) / 79};
                FVector3 Position{Origin};
                // Same reproducible moving paths for every candidate; far view also exercises subpixel culling.
                if (Path == 0) { Position.mX += T * 12; Position.mY += T * 8; }
                if (Path == 1) { Position = {-8 + T*12, -8 + T*8, 6}; }
                if (Path == 2) { Position.mX -= 150 + T*150; Position.mY -= 150 + T*150; Position.mZ += 100 + T*100; }
                const auto World{UCameraComponent::CameraBasis * FTransform{Position, Rotation, {1,1,1}}.ToMatrixWithScale()};
                View.mCamera.mView = World.Invert();
                View.mCamera.mProjection = FMatrix::CreatePerspectiveFieldOfView(C["FOV"][0].GetFloat()*PI/180, float(Width)/Height, C["NearClip"][0].GetFloat(), Path == 2 ? 2000.0f : C["FarClip"][0].GetFloat());
                View.mCamera.mViewProjection = View.mCamera.mView * View.mCamera.mProjection;
                FFrustum Local; FFrustum::CreateFromMatrix(Local, View.mCamera.mProjection.ToSimpleMath()); Local.Transform(View.mCamera.mViewFrustum, World.ToSimpleMath());
                const auto Begin{Clock::now()};
                Queue.Build(&Registry, Scene, View); const double QueueMS{Milliseconds(Begin)};
                Require(Frame.BeginFrame(Context.Get(), 0) && Frame.PrepareView(Device.Get(), Context.Get(), View, Scene, Queue), "prepare view");
                Surface.Bind(Context.Get()); const float Clear[]{0.12f,0.14f,0.18f,1}; Surface.Clear(Context.Get(), Clear);
                Context->Begin(Disjoint.Get()); Context->End(StartGPU.Get());
                Renderer.Draw({Context.Get(), &Registry, *Materials.GetSRV(), &Frame}, Queue.GetItems(ERenderPass::SceneGeometry), ERenderMode::Lit);
                Context->End(EndGPU.Get()); Context->End(Disjoint.Get()); Frame.EndFrame();
                Context->End(Done.Get()); Context->Flush(); const double SubmitMS{Milliseconds(Begin)};
                while (Context->GetData(Done.Get(), nullptr, 0, 0) == S_FALSE) { SwitchToThread(); }
                const double TotalMS{Milliseconds(Begin)};
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Timing{}; UINT64 A{}, B{};
                Require(Context->GetData(Disjoint.Get(), &Timing, sizeof(Timing), 0) == S_OK && !Timing.Disjoint, "GPU timing");
                Require(Context->GetData(StartGPU.Get(), &A, sizeof(A), 0) == S_OK && Context->GetData(EndGPU.Get(), &B, sizeof(B), 0) == S_OK, "timestamps");
                if (FrameIndex >= 20) {
                    Prepare.push_back(QueueMS); CPU.push_back(SubmitMS); Total.push_back(TotalMS); GPU.push_back(double(B-A)*1000/Timing.Frequency);
                    for (const auto& Batch : Queue.GetItems(ERenderPass::SceneGeometry)) { Triangles += double(Batch.mState.mIndexCount/3) * Batch.mRecordCount; }
                    Draws += Renderer.GetLastDrawStats().mDrawCallCount; Records += Queue.GetDrawRecords().size();
                }
                if (FrameIndex == 50) {
                    ComPtr<ID3D11Resource> Color; Surface.GetShaderResourceView()->GetResource(&Color);
                    DirectX::ScratchImage Image; Require(SUCCEEDED(DirectX::CaptureTexture(Device.Get(), Context.Get(), Color.Get(), Image)), "capture");
                    const std::wstring File{(OutputDirectory / ("LOD-" + Label + "-" + std::to_string(Path) + ".png")).wstring()};
                    Require(SUCCEEDED(DirectX::SaveToWICFile(*Image.GetImage(0,0,0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), File.c_str())), "PNG");
                }
            }
            auto Mean = [](const auto& Values) { return std::accumulate(Values.begin(), Values.end(), 0.0)/Values.size(); };
            CSV << Path << ',' << Width << ',' << Height << ',' << Index << ',' << Mean(Prepare) << ',' << Mean(CPU) << ',' << Mean(GPU) << ',' << Mean(Total) << ',' << 1000/Mean(Total) << ',' << Triangles/80 << ',' << Draws/80 << ',' << Records/80 << '\n'; CSV.flush();
            std::cout << Label << " path=" << Path << " queue_ms=" << Mean(Prepare) << " cpu_ms=" << Mean(CPU) << " gpu_ms=" << Mean(GPU) << " serial_ms=" << Mean(Total) << " triangles=" << Triangles/80 << " draws=" << Draws/80 << std::endl;
        }
        Materials.Reset(); Context->ClearState();
    } catch (const std::exception& E) { std::cerr << E.what() << '\n'; return 1; }
}
