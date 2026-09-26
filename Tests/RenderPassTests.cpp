#include "pch.h"
#include "Render/Renderer.h"
#include "Asset/UMesh.h"
#include "Asset/UFont.h"
#include <d3d11sdklayers.h>
#include <iostream>
#include <stdexcept>

namespace {
    bool IsColored(Uint32 Pixel) {
        return (Pixel & 0x00ffffff) != 0;
    }

    bool IsClear(Uint32 Pixel) {
        return !IsColored(Pixel);
    }

    bool HasSameDepth(Uint32 Left, Uint32 Right) {
        return (Left & 0x00ffffff) == (Right & 0x00ffffff);
    }

    void Check(bool Condition, const char* Message) {
        if (!Condition) {
            throw std::runtime_error{Message};
        }
    }

    std::vector<Uint32> ReadTexture(FRenderer& Renderer, ID3D11Resource* Resource) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture{};
        Check(SUCCEEDED(Resource->QueryInterface(IID_PPV_ARGS(Texture.GetAddressOf()))), "Texture query failed");
        D3D11_TEXTURE2D_DESC Description{};
        Texture->GetDesc(&Description);
        Description.Usage = D3D11_USAGE_STAGING;
        Description.BindFlags = 0;
        Description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> Staging{};
        Check(SUCCEEDED(Renderer.GetDevice()->CreateTexture2D(&Description, nullptr, Staging.GetAddressOf())), "Staging texture creation failed");
        Renderer.GetDeviceContext()->CopyResource(Staging.Get(), Texture.Get());
        D3D11_MAPPED_SUBRESOURCE Mapped{};
        Check(SUCCEEDED(Renderer.GetDeviceContext()->Map(Staging.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "Texture readback failed");
        std::vector<Uint32> Pixels{};
        Pixels.resize(static_cast<std::size_t>(Description.Width) * Description.Height);
        for (Uint32 Row{}; Row < Description.Height; ++Row) {
            std::memcpy(Pixels.data() + static_cast<std::size_t>(Row) * Description.Width, static_cast<const Uint8*>(Mapped.pData) + static_cast<std::size_t>(Row) * Mapped.RowPitch, Description.Width * sizeof(Uint32));
        }
        Renderer.GetDeviceContext()->Unmap(Staging.Get(), 0);
        return Pixels;
    }

    std::vector<Uint32> ReadColor(FRenderer& Renderer, FSceneRenderSurface& Surface) {
        Microsoft::WRL::ComPtr<ID3D11Resource> Resource{};
        Surface.GetShaderResourceView()->GetResource(Resource.GetAddressOf());
        return ReadTexture(Renderer, Resource.Get());
    }

    std::vector<Uint32> ReadDepth(FRenderer& Renderer) {
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> Depth{};
        Renderer.GetDeviceContext()->OMGetRenderTargets(0, nullptr, Depth.GetAddressOf());
        Check(Depth != nullptr, "Scene depth was not rebound after the pass sequence");
        Microsoft::WRL::ComPtr<ID3D11Resource> Resource{};
        Depth->GetResource(Resource.GetAddressOf());
        return ReadTexture(Renderer, Resource.Get());
    }

    void CheckDebugMessages(ID3D11InfoQueue* Queue) {
        for (UINT64 Index{}; Index < Queue->GetNumStoredMessages(); ++Index) {
            SIZE_T Size{};
            Queue->GetMessage(Index, nullptr, &Size);
            std::vector<Uint8> Storage{};
            Storage.resize(Size);
            D3D11_MESSAGE* Message{reinterpret_cast<D3D11_MESSAGE*>(Storage.data())};
            Queue->GetMessage(Index, Message, &Size);
            if (Message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING) {
                throw std::runtime_error{Message->pDescription};
            }
        }
    }

    void TestPasses(FRenderer& Renderer, FAssetRegistry& Registry) {
        FSceneRenderSurface Surface{};
        Surface.InitializeOffscreen(Renderer.GetDevice(), 128, 128);
        FRenderView View{};
        View.mTarget = &Surface;
        View.mSettings.mClearColor = FVector4{0.0f, 0.0f, 0.0f, 1.0f};
        View.mCamera.mView = FMatrix::CreateTranslation(0.0f, 0.0f, 4.0f);
        View.mCamera.mProjection = FMatrix::CreateOrthographic(4.0f, 4.0f, 0.1f, 100.0f);
        View.mCamera.mViewProjection = View.mCamera.mView * View.mCamera.mProjection;
        View.mPasses.reset();
        View.SetPassEnabled(ERenderPass::SelectionOutline, true);
        Check(!View.IsPassEnabled(ERenderPass::SelectionOutline), "Outline must require scene geometry");
        View.SetPassEnabled(ERenderPass::SceneGeometry, true);
        Check(View.IsPassEnabled(ERenderPass::SelectionOutline), "Outline dependency was not satisfied");
        View.SetPassEnabled(ERenderPass::Count, true);
        Check(!View.IsPassEnabled(ERenderPass::Count), "Invalid passes must stay disabled");

        FRenderProbe Probe{};
        FActorProbe Actor{};
        Actor.mMeshHandle = Registry.FindAsset(FAssetPath{"/Game/System/Mesh/Cube.bin"});
        Actor.mMaterialHandle = Registry.EnsureDefaultStaticMeshMaterial();
        Actor.mPipelineHandle = Registry.EnsureDefaultStaticMeshPipeline();
        Actor.mFlags = static_cast<Uint32>(ERenderObjectFlags::Selected);
        Check(Registry.ResolveAsset<UMesh>(Actor.mMeshHandle) != nullptr, "Test cube is missing");
        UPipeline* Pipeline{Registry.ResolveAsset<UPipeline>(Actor.mPipelineHandle)};
        Check(Pipeline != nullptr, "Test pipeline is missing");
        Probe.mActorProbes.push_back(Actor);
        Probe.mBForceUnlit = true;

        FRenderQueue DrawQueue{};
        DrawQueue.Build(Registry, View, Probe);
        Check(!DrawQueue.GetItems(ERenderPass::SceneGeometry).empty(), "Scene queue is empty");
        Check(DrawQueue.GetItems(ERenderPass::SceneGeometry).size() == DrawQueue.GetItems(ERenderPass::SelectionOutline).size(), "Selected submeshes are missing from outline queue");
        Check(Probe.mActorProbes.front().mFlags == Actor.mFlags, "Queue building mutated input flags");
        Check((DrawQueue.GetItems(ERenderPass::SceneGeometry).front().mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Unlit)) != 0, "Unlit override did not reach draw items");

        Probe.mBForceUnlit = false;
        View.mRenderMode = ERenderMode::Unlit;
        DrawQueue.Build(Registry, View, Probe);
        Check((DrawQueue.GetItems(ERenderPass::SceneGeometry).front().mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Unlit)) != 0, "View unlit mode was ignored");
        View.mRenderMode = ERenderMode::Lit;
        DrawQueue.Build(Registry, View, Probe);
        Check((DrawQueue.GetItems(ERenderPass::SceneGeometry).front().mProbe.mFlags & static_cast<Uint32>(ERenderObjectFlags::Unlit)) == 0, "Unlit view contaminated the next lit view");
        Probe.mBForceUnlit = true;

        Pipeline->SetRenderMode(ERenderMode::Wireframe);
        Renderer.BeginFrame(0.016f);
        Renderer.RenderView(View, Probe);
        Check(Pipeline->GetRenderMode() == ERenderMode::Wireframe, "Rendering mutated shared pipeline mode");
        const std::vector<Uint32> SceneColor{ReadColor(Renderer, Surface)};
        const std::vector<Uint32> SceneDepth{ReadDepth(Renderer)};
        Check(std::ranges::any_of(SceneColor, IsColored), "Scene did not draw any visible pixels");

        View.SetPassEnabled(ERenderPass::Gizmo, true);
        Renderer.RenderView(View, Probe);
        Check(ReadDepth(Renderer) == SceneDepth, "An empty gizmo pass cleared scene depth");

        FActorProbe Gizmo{Actor};
        Gizmo.mWorld = FMatrix::CreateTranslation(1.0f, 0.0f, 0.0f);
        Gizmo.mPipelineHandle = Registry.FindAsset(FAssetPath{"/Game/Pipeline/Gizmo.json"});
        Probe.mGizmoProbes.push_back(Gizmo);
        Renderer.RenderView(View, Probe);
        const std::vector<Uint32> SceneAndGizmoDepth{ReadDepth(Renderer)};
        Check(!std::ranges::equal(SceneAndGizmoDepth, SceneDepth, HasSameDepth), "Gizmo pass did not replace scene depth");
        Check(ReadColor(Renderer, Surface) != SceneColor, "Gizmo pass produced no visible output");
        View.mPasses.reset();
        View.SetPassEnabled(ERenderPass::Gizmo, true);
        Renderer.RenderView(View, Probe);
        Check(std::ranges::equal(ReadDepth(Renderer), SceneAndGizmoDepth, HasSameDepth), "Scene depth was not cleared before drawing gizmos");
        View.SetPassEnabled(ERenderPass::OrientationAxis, true);
        Renderer.RenderView(View, Probe);
        Check(Probe.mActorProbes.front().mFlags == Actor.mFlags, "Rendering mutated input flags");

        View.mPasses.reset();
        Renderer.RenderView(View, Probe);
        Check(std::ranges::all_of(ReadColor(Renderer, Surface), IsClear), "Disabled passes still drew content");

        View.SetPassEnabled(ERenderPass::SceneGuides, true);
        Probe.mSceneGuides.AddLine(FVector3{-1.0f, 0.0f, 0.0f}, FVector3{1.0f, 0.0f, 0.0f}, FVector4{1.0f, 0.0f, 0.0f, 1.0f}, 3.0f);
        Renderer.RenderView(View, Probe);
        Check(std::ranges::any_of(ReadColor(Renderer, Surface), IsColored), "Scene guide pass failed when mesh passes were disabled");

        FBillboardProbe Billboard{};
        Billboard.mTextureHandle = Registry.FindAsset(FAssetPath{"/Game/Texture/checkerboard.png"});
        Billboard.mPipelineHandle = Registry.FindAsset(FAssetPath{"/Game/Pipeline/Billboard.json"});
        Billboard.mSize = FVector2{2.0f, 2.0f};
        Billboard.mUvMax = FVector2{1.0f, 1.0f};
        Billboard.mColor = FVector4{1.0f, 1.0f, 1.0f, 1.0f};
        Probe.mBillboardProbes.push_back(Billboard);
        View.mPasses.reset();
        View.SetPassEnabled(ERenderPass::Billboard, true);
        Renderer.GetDeviceContext()->ClearState();
        Renderer.RenderView(View, Probe);
        Check(std::ranges::any_of(ReadColor(Renderer, Surface), IsColored), "Billboard pass depends on earlier pass state");

        FTextProbe Text{};
        Text.mFontHandle = Registry.FindAsset(FAssetPath{"/Game/Font/NotoSansKR-Medium.ttf"});
        Text.mPipelineHandle = Registry.FindAsset(FAssetPath{"/Game/Pipeline/Text.json"});
        const FFontGlyph* Glyph{Registry.GetOrCreateFontGlyph(Text.mFontHandle, U'A')};
        Check(Glyph != nullptr, "Test font glyph is missing");
        Text.mVertices.push_back(FTextVertex{FVector2{-0.5f, 0.5f}, FVector2{1.0f, 1.0f}, Glyph->mUvMin, Glyph->mUvMax});
        Probe.mTextProbes.push_back(Text);
        View.mPasses.reset();
        View.SetPassEnabled(ERenderPass::Text, true);
        Renderer.GetDeviceContext()->ClearState();
        Renderer.RenderView(View, Probe);
        Check(std::ranges::any_of(ReadColor(Renderer, Surface), IsColored), "Text pass depends on earlier pass state");

        FSceneRenderSurface OtherSurface{};
        OtherSurface.InitializeOffscreen(Renderer.GetDevice(), 64, 96);
        View.mTarget = &OtherSurface;
        View.mPasses.reset();
        View.SetPassEnabled(ERenderPass::OrientationAxis, true);
        Renderer.RenderView(View, Probe);
        UINT ViewportCount{1};
        D3D11_VIEWPORT Viewport{};
        Renderer.GetDeviceContext()->RSGetViewports(&ViewportCount, &Viewport);
        Check(Viewport.Width == 64.0f && Viewport.Height == 96.0f, "Orientation axis leaked its viewport");
        Check(std::ranges::any_of(ReadColor(Renderer, OtherSurface), IsColored), "Orientation pass failed after changing target size");
        View.mTarget = &Surface;
        View.mPasses.reset();
        View.SetPassEnabled(ERenderPass::SceneGeometry, true);
        View.SetPassEnabled(ERenderPass::SelectionOutline, true);
        Renderer.RenderView(View, Probe);
        Check(ReadColor(Renderer, Surface) == SceneColor, "Rendering another view changed the original view result");
        Pipeline->SetRenderMode(ERenderMode::Lit);
    }
}

int main() {
    try {
        Check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "COM initialization failed");
        const HWND Window{CreateWindowExW(0, L"STATIC", L"Render pass tests", WS_OVERLAPPEDWINDOW, 0, 0, 256, 256, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr)};
        Check(Window != nullptr, "Hidden test window creation failed");
        FRenderer Renderer{};
        Renderer.Create(Window, 256, 256);
        Check(Renderer.Initialize(), "Renderer initialization failed");
        FAssetRegistry Registry{};
        Registry.Initialize(Renderer.GetDevice());
        Renderer.BindAssetRegistry(&Registry);
        Microsoft::WRL::ComPtr<ID3D11InfoQueue> Queue{};
        Check(SUCCEEDED(Renderer.GetDevice()->QueryInterface(IID_PPV_ARGS(Queue.GetAddressOf()))), "Debug layer is required for render pass tests");
        Queue->ClearStoredMessages();
        TestPasses(Renderer, Registry);
        CheckDebugMessages(Queue.Get());
        Renderer.Terminate();
        Registry.Reset();
        DestroyWindow(Window);
        CoUninitialize();
        std::cout << "Render pass tests passed\n";
        return 0;
    } catch (const std::exception& Error) {
        std::cerr << Error.what() << '\n';
        return 1;
    }
}
