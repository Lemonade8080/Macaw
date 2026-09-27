#include "pch.h"
#include "FViewerPanel.h"
#include "Editor/FileDialog.h"

#include "ImGui/imgui.h"
#include "Render/Renderer.h"
#include "Asset/FAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"
#include "Asset/USurfaceOpaque.h"
#include "Core/Console/Console.h"
#include "Asset/Pipeline/UPipeline.h"
#include "World/FWorldEditorContext.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float MinimumDistance{0.10f};
    constexpr float MaximumDistance{1000000.0f};
    constexpr char DefaultMeshPath[]{"/Game/System/Mesh/Cube.bin"};
    constexpr char DefaultMaterialPath[]{"/Game/System/Material/Green.mtl"};
    constexpr char DefaultPipelinePath[]{"/Game/Pipeline/Base"};
    constexpr char TexturedPipelinePath[]{"/Game/Pipeline/TexturedBase"};
}

FViewerPanel::FViewerPanel(FAssetRegistry& InRegistry, HWND InputWindowHandle, FMessageChannel::FSender InEditorToWorldSender, FWorldEditorContext& InEditorContext, FAssetThumbnailRenderer* InThumbnailRenderer)
	: FEditorWindow{"Viewer", ImGuiWindowFlags_MenuBar},
	  mRegistry{&InRegistry},
	  mEditorToWorldSender{std::move(InEditorToWorldSender)},
	  mWindowHandle{InputWindowHandle},
	  mEditorContext{InEditorContext} {
    mPropertyEditor.BindAssetRegistry(&InRegistry);
    mPropertyEditor.BindThumbnailRenderer(InThumbnailRenderer);
    SetMesh({});
    SetMaterial({});
}

void FViewerPanel::SetMesh(FAssetHandle InMeshHandle) {
    mMeshHandle = mRegistry->ResolveAsset<UMesh>(InMeshHandle) != nullptr ? InMeshHandle : mRegistry->FindAsset(FAssetPath{DefaultMeshPath});
}

void FViewerPanel::SetMaterial(FAssetHandle InMaterialHandle) {
    mMaterialHandle = mRegistry->ResolveAsset<UMaterial>(InMaterialHandle) != nullptr ? InMaterialHandle : mRegistry->FindAsset(FAssetPath{DefaultMaterialPath});
    if (mRegistry->ResolveAsset<UMaterial>(mMaterialHandle) == nullptr) {
        mMaterialHandle = mRegistry->EnsureDefaultStaticMeshMaterial();
    }
}

bool FViewerPanel::OpenViewerFile(const std::filesystem::path& FilePath) {
    if (mRegistry == nullptr) {
        return false;
    }

    std::string Extension{FilePath.extension().string()};
    std::ranges::transform(Extension, Extension.begin(), [](unsigned char Character) {
        return static_cast<char>(std::tolower(Character));
    });
    if (Extension != ".obj" && Extension != ".bin" && Extension != ".mtl") {
        return false;
    }

    const FAssetHandle Handle{mRegistry->LoadViewerAsset(FilePath)};
    if (Extension == ".mtl" && mRegistry->ResolveAsset<UMesh>(mMeshHandle) != nullptr && mRegistry->ResolveAsset<UMaterial>(Handle) != nullptr) {
        SetMaterial(Handle);
        return true;
    }
    if (Extension != ".mtl" && mRegistry->ResolveAsset<UMesh>(Handle) != nullptr) {
        SetMesh(Handle);
        const UMesh* Mesh{mRegistry->ResolveAsset<UMesh>(mMeshHandle)};
        const Uint32 VertexCount{Mesh->GetVertexAttributeCount(EVertexAttribute::Position)};
        const Uint32 VertexStride{Mesh->GetVertexStride(EVertexAttribute::Position)};
        const std::byte* VertexData{static_cast<const std::byte*>(Mesh->GetVertexData(EVertexAttribute::Position))};
        if (VertexData != nullptr && VertexCount > 0 && VertexStride >= sizeof(FVector3)) {
            FVector3 Minimum{*reinterpret_cast<const FVector3*>(VertexData)};
            FVector3 Maximum{Minimum};
            for (Uint32 Index{1}; Index < VertexCount; ++Index) {
                const FVector3& Position{*reinterpret_cast<const FVector3*>(VertexData + static_cast<std::size_t>(Index) * VertexStride)};
                Minimum.mX = std::min(Minimum.mX, Position.mX);
                Minimum.mY = std::min(Minimum.mY, Position.mY);
                Minimum.mZ = std::min(Minimum.mZ, Position.mZ);
                Maximum.mX = std::max(Maximum.mX, Position.mX);
                Maximum.mY = std::max(Maximum.mY, Position.mY);
                Maximum.mZ = std::max(Maximum.mZ, Position.mZ);
            }
            mTarget = (Minimum + Maximum) * 0.5f;
            mDistance = std::clamp((Maximum - Minimum).Length() / std::tan(mFieldOfView * 0.5f), MinimumDistance, MaximumDistance);
        }
        return true;
    }
    return false;
}

bool FViewerPanel::HandleExternalFileDrop(const std::filesystem::path& FilePath, const ImVec2& ScreenPosition) {
    if (!IsVisible() || ScreenPosition.x < mDropTargetMin.x || ScreenPosition.x >= mDropTargetMax.x || ScreenPosition.y < mDropTargetMin.y || ScreenPosition.y >= mDropTargetMax.y) {
        return false;
    }
    return OpenViewerFile(FilePath);
}

void FViewerPanel::DrawContents() {
    mDropTargetMin = ImGui::GetWindowPos();
    const ImVec2 WindowSize{ImGui::GetWindowSize()};
    mDropTargetMax = ImVec2{mDropTargetMin.x + WindowSize.x, mDropTargetMin.y + WindowSize.y};
    if (const FAssetHandle PreviewMesh{mEditorContext.ConsumePreviewMesh()}; PreviewMesh) {
        SetMesh(PreviewMesh);
    }

    DrawMenuBar();
    DrawProperties();
    ImGui::Separator();
    DrawPreview();
}

void FViewerPanel::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
#ifdef OBJ_VIEWER
        if (ImGui::MenuItem("Open Model or Material...")) {
#else
        if (ImGui::MenuItem("Import OBJ...")) {
#endif
#ifdef OBJ_VIEWER
            FString FilePath{OpenFileDialog(mWindowHandle, "./Content/ModelingFiles", "Model and Material Files\0*.bin;*.obj;*.mtl\0All Files\0*.*\0")};
#else
            FString FilePath{OpenFileDialog(mWindowHandle, "./Content/ModelingFiles", "OBJ Files(*.obj)\0*.obj\0All Files(*.*)\0*.*\0", "obj")};
#endif
            if (!FilePath.empty()) {
#ifdef OBJ_VIEWER
                OpenViewerFile(std::filesystem::path{FilePath.c_str()});
#else
                mEditorToWorldSender.TryEmplace<FMessageImportMesh>(FString{"ObjImport"}, std::move(FilePath), FString{});
#endif
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void FViewerPanel::DrawProperties() {
    if (mRegistry == nullptr) {
        mPropertyEditor.DrawDisabledText("Asset registry unavailable");
        return;
    }

    ImGui::TextUnformatted("Preview Assets");
    mPropertyEditor.DrawAssetPicker("StaticMesh", *UMesh::StaticTypeInfo(), mMeshHandle, [this](FAssetHandle Handle) {
        SetMesh(Handle);
    });
    mPropertyEditor.DrawAssetPicker("Material", *UMaterial::StaticTypeInfo(), mMaterialHandle, [this](FAssetHandle Handle) {
        SetMaterial(Handle);
    });
}

void FViewerPanel::ResizeSurfaceIfNeeded(ID3D11Device* Device, Uint32 Width, Uint32 Height) {
    if (Device == nullptr || Width == 0 || Height == 0) {
        return;
    }

    if (!mSurface.IsValid()) {
        mSurface.InitializeOffscreen(Device, Width, Height);
    } else if (Width != mSurfaceWidth || Height != mSurfaceHeight) {
        mSurface.Resize(Device, Width, Height);
    }

    mSurfaceWidth = Width;
    mSurfaceHeight = Height;
}

FMatrix FViewerPanel::MakeCameraWorldMatrix(const FVector3& Eye) const {
    FVector3 Forward{mTarget - Eye};
    Forward.Normalize();
    FVector3 Right{FVector3{0.0f, 0.0f, 1.0f}.Cross(Forward)};
    Right.Normalize();
    FVector3 Up{Forward.Cross(Right)};
    Up.Normalize();

    FMatrix Result{FMatrix::Identity};
    Result.m_[0][0] = Right.mX;
    Result.m_[0][1] = Right.mY;
    Result.m_[0][2] = Right.mZ;
    Result.m_[1][0] = Up.mX;
    Result.m_[1][1] = Up.mY;
    Result.m_[1][2] = Up.mZ;
    Result.m_[2][0] = Forward.mX;
    Result.m_[2][1] = Forward.mY;
    Result.m_[2][2] = Forward.mZ;
    Result.m_[3][0] = Eye.mX;
    Result.m_[3][1] = Eye.mY;
    Result.m_[3][2] = Eye.mZ;
    return Result;
}

FRenderProbe FViewerPanel::BuildPreviewProbe() {
    FRenderProbe Probe{};
    if (mRegistry == nullptr || mSurfaceWidth == 0 || mSurfaceHeight == 0) {
        return Probe;
    }

    if (mRegistry->ResolveAsset<UMesh>(mMeshHandle) == nullptr) {
        SetMesh({});
    }
    if (mRegistry->ResolveAsset<UMaterial>(mMaterialHandle) == nullptr) {
        SetMaterial({});
    }

    bool HasTexture{};
    if (const USurfaceOpaque* Material{mRegistry->ResolveAsset<USurfaceOpaque>(mMaterialHandle)}; Material != nullptr) {
        for (Uint32 GroupIndex{}; GroupIndex < Material->GetGroups().size() && !HasTexture; ++GroupIndex) {
            const FMaterialChunkSignature Signature{Material->BuildChunkSignature(GroupIndex)};
            for (Uint8 TextureIndex{}; TextureIndex < Signature.mTextureFieldCount; ++TextureIndex) {
                HasTexture = HasTexture || static_cast<bool>(Signature.GetTextureHandle(TextureIndex));
            }
        }
    }
    const FAssetHandle PipelineHandle{mRegistry->FindAsset(FAssetPath{HasTexture ? TexturedPipelinePath : DefaultPipelinePath})};
    UPipeline* Pipeline{mRegistry->ResolveAsset<UPipeline>(PipelineHandle)};
    if (mMeshHandle && mMaterialHandle && Pipeline != nullptr) {
        FActorProbe ActorProbe{};
        ActorProbe.mMeshHandle = mMeshHandle;
        ActorProbe.mMaterialHandle = mMaterialHandle;
        ActorProbe.mPipelineHandle = PipelineHandle;
        Probe.mActorProbes.push_back(ActorProbe);
    }

    FLightProbe LightProbe{};
    LightProbe.mType = ELightType::Directional;
    LightProbe.mColor = FVector3{1.0f, 1.0f, 1.0f};
    LightProbe.mIntensity = 1.0f;
    FVector3 LightDirection{-FMatrix::CreateFromQuaternion(mOrbitRotation).TransformDirection(-FVector::UnitX) - FVector::UnitZ * 0.75f};
    LightDirection.Normalize();
    LightProbe.mDirection = LightDirection;
    Probe.mLightProbes.push_back(LightProbe);
    return Probe;
}

CameraProbe FViewerPanel::BuildPreviewCamera() const {
    const FMatrix OrbitMatrix{FMatrix::CreateFromQuaternion(mOrbitRotation)};
    const FVector3 Offset{OrbitMatrix.TransformDirection(-FVector::UnitX)};
    const FVector3 Eye{mTarget + Offset * mDistance};
    const float Aspect{static_cast<float>(mSurfaceWidth) / static_cast<float>(mSurfaceHeight)};
    CameraProbe Camera{};
    Camera.mView = MakeCameraWorldMatrix(Eye).Invert();
    Camera.mProjection = FMatrix::CreatePerspectiveFieldOfView(mFieldOfView, Aspect, 0.1f, std::max(1000.0f, mDistance * 4.0f));
    Camera.mViewProjection = Camera.mView * Camera.mProjection;
    return Camera;
}

void FViewerPanel::ProcessInput() {
    const ImGuiIO& Input{ImGui::GetIO()};
    if (ImGui::IsItemActive()) {
        const float DeltaYaw{Input.MouseDelta.x * 0.01f};
        const float DeltaPitch{Input.MouseDelta.y * 0.01f};
        const FQuat YawRotation{FQuat::CreateFromAxisAngle(FVector::UnitZ, DeltaYaw)};
        const FMatrix CurrentRotation{FMatrix::CreateFromQuaternion(mOrbitRotation)};
        const FVector Right{CurrentRotation.TransformDirection(FVector::UnitY)};
        const FQuat PitchRotation{FQuat::CreateFromAxisAngle(Right, DeltaPitch)};
        mOrbitRotation = PitchRotation * YawRotation * mOrbitRotation;
        mOrbitRotation.Normalize();
    }

    if (ImGui::IsItemHovered() && Input.MouseWheel != 0.0f) {
        mDistance *= std::pow(0.9f, Input.MouseWheel);
        mDistance = std::clamp(mDistance, MinimumDistance, MaximumDistance);
    }
}

void FViewerPanel::RenderOffscreen(FRenderer& InRenderer, FAssetRegistry&) {
    if (mDesiredWidth == 0 || mDesiredHeight == 0) {
        return;
    }

    ResizeSurfaceIfNeeded(InRenderer.GetDevice(), mDesiredWidth, mDesiredHeight);
    if (!mSurface.IsValid()) {
        return;
    }

    FRenderProbe PreviewProbe{BuildPreviewProbe()};
    FRenderSettings PreviewSettings{};
    PreviewSettings.mClearColor = FVector4{0.12f, 0.13f, 0.15f, 1.0f};
    FRenderView View{};
    View.mTarget = &mSurface;
    View.mCamera = BuildPreviewCamera();
    View.mSettings = PreviewSettings;
    View.mOrientationAxisSize = 100.0f;
    View.mPasses.reset();
    View.SetPassEnabled(ERenderPass::SceneGeometry, true);
    View.SetPassEnabled(ERenderPass::OrientationAxis, true);
    InRenderer.RenderView(View, PreviewProbe);
}

void FViewerPanel::ReleaseRenderResources() {
    mSurface.Reset();
}

void FViewerPanel::DrawPreview() {
    const ImVec2 Available{ImGui::GetContentRegionAvail()};
    mDesiredWidth = static_cast<Uint32>(std::max(1.0f, Available.x));
    mDesiredHeight = static_cast<Uint32>(std::max(1.0f, Available.y));
    const ImVec2 Size{static_cast<float>(mDesiredWidth), static_cast<float>(mDesiredHeight)};
    const ImVec2 TopLeft{ImGui::GetCursorScreenPos()};
    const ImGuiViewport* Viewport{ImGui::GetWindowViewport()};
    ImGui::InvisibleButton("##PreviewViewport", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    if (ID3D11ShaderResourceView* PreviewSRV{mSurface.GetShaderResourceView()}; PreviewSRV != nullptr) {
        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(PreviewSRV), TopLeft, ImVec2{TopLeft.x + Size.x, TopLeft.y + Size.y});
    }

    ProcessInput();

    Uint32 VertexCount{};
    std::size_t TriangleCount{};
    if (const UMesh* Mesh{mRegistry != nullptr ? mRegistry->ResolveAsset<UMesh>(mMeshHandle) : nullptr}; Mesh != nullptr) {
        VertexCount = Mesh->GetVertexAttributeCount(EVertexAttribute::Position);
        TriangleCount = Mesh->GetIndices().size() / 3;
    }

    constexpr float OverlayMargin{12.0f};
    ImGui::SetNextWindowViewport(Viewport->ID);
    ImGui::SetNextWindowPos(ImVec2{TopLeft.x + Size.x - OverlayMargin, TopLeft.y + OverlayMargin}, ImGuiCond_Always, ImVec2{1.0f, 0.0f});
    ImGui::SetNextWindowBgAlpha(0.4f);
    const ImGuiWindowFlags OverlayFlags{ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize};
    if (ImGui::Begin("Mesh Statistics##Viewer", nullptr, OverlayFlags)) {
        ImGui::Text("Vertices: %u", VertexCount);
        ImGui::Text("Triangles: %zu", TriangleCount);
    }
    ImGui::End();
}
