#include "pch.h"

#include "FAssetThumbnailRenderer.h"
#include "Render/Renderer.h"
#include "Core/Stat/Stat.h"

#include "Asset/UMaterial.h"
#include "Asset/UMesh.h"

#include <algorithm>

namespace {
    constexpr Uint32 ThumbnailSize{256};

    constexpr const char* DefaultMaterialPath{"/Game/System/Material/Default.mtl"};
    constexpr const char* SphereMeshPath{"/Game/System/Mesh/Sphere.bin"};
    constexpr const char* StaticMeshPipelinePath{"/Game/Pipeline/Base"};
    constexpr const char* MaterialPipelinePath{"/Game/Pipeline/TexturedBase"};
}

void FAssetThumbnailRenderer::Create(FRenderer* InRenderer, FAssetRegistry* InAssetRegistry) {
    mRenderer = InRenderer;
    mAssetRegistry = InAssetRegistry;

    if (mRenderer == nullptr || mAssetRegistry == nullptr) {
        return;
    }

    mPendingAssetHandles.clear();
    mPendingAssetIndex = 0;

    for (const FAssetEntry& Entry : mAssetRegistry->GetAssetEntries()) {
        if (Entry.mAsset == nullptr) {
            continue;
        }

        if (Entry.mAssetType != EAssetType::Mesh && Entry.mAssetType != EAssetType::Material) {
            continue;
        }

        mPendingAssetHandles.push_back(Entry.mHandle);
    }
}

void FAssetThumbnailRenderer::Tick(Uint32 MaxThumbnailCount) {
    Uint32 RenderedThumbnailCount{};

    while (mPendingAssetIndex < mPendingAssetHandles.size() && RenderedThumbnailCount < MaxThumbnailCount) {
        RenderThumbnail(mPendingAssetHandles[mPendingAssetIndex]);
        ++mPendingAssetIndex;
        ++RenderedThumbnailCount;
    }

    if (mPendingAssetIndex == mPendingAssetHandles.size()) {
        mPendingAssetHandles.clear();
        mPendingAssetIndex = 0;
    }
}

float FAssetThumbnailRenderer::GetGenerationProgress() const {
    return mPendingAssetHandles.empty() ? 1.0f : static_cast<float>(mPendingAssetIndex) / static_cast<float>(mPendingAssetHandles.size());
}

void FAssetThumbnailRenderer::RenderThumbnail(FAssetHandle AssetHandle) {
    const FAssetEntry* Entry{FindAssetEntry(AssetHandle)};

    if (Entry == nullptr) {
        return;
    }

    RenderThumbnail(*Entry);
}

void FAssetThumbnailRenderer::RenderMaterialPreview(FAssetHandle MaterialHandle, FSceneRenderSurface& Surface) {
    const FAssetEntry* Entry{FindAssetEntry(MaterialHandle)};
    if (Entry == nullptr || Entry->mAssetType != EAssetType::Material || !Surface.IsValid()) {
        return;
    }

    RenderThumbnail(*Entry, &Surface);
}

void FAssetThumbnailRenderer::RenderThumbnail(const FAssetEntry& Entry, FSceneRenderSurface* PreviewSurface) {
    const Stat::FScopedSystemStatTimer StageStat{ Stat::ESystemStatStage::PreviewRender };
    if (mRenderer == nullptr || mAssetRegistry == nullptr || Entry.mAsset == nullptr) {
        return;
    }

    FActorProbe ActorProbe{};

    if (Entry.mAssetType == EAssetType::Mesh) {
        ActorProbe.mMeshHandle = Entry.mHandle;
        ActorProbe.mMaterialHandle = mAssetRegistry->FindAsset(FAssetPath{DefaultMaterialPath});
        ActorProbe.mPipelineHandle = mAssetRegistry->FindAsset(FAssetPath{StaticMeshPipelinePath});
    } else if (Entry.mAssetType == EAssetType::Material) {
        ActorProbe.mMeshHandle = mAssetRegistry->FindAsset(FAssetPath{SphereMeshPath});
        ActorProbe.mMaterialHandle = Entry.mHandle;
        ActorProbe.mPipelineHandle = mAssetRegistry->FindAsset(FAssetPath{MaterialPipelinePath});
    } else {
        return;
    }

    UMesh* Mesh{mAssetRegistry->ResolveAsset<UMesh>(ActorProbe.mMeshHandle)};

    UMaterial* Material{mAssetRegistry->ResolveAsset<UMaterial>(ActorProbe.mMaterialHandle)};

    if (Mesh == nullptr || Material == nullptr || !ActorProbe.mPipelineHandle) {
        return;
    }

    ActorProbe.mWorld = BuildMeshTransform(*Mesh);
    Mesh->GetBoundingBox().Transform(ActorProbe.mWorldOBB, ActorProbe.mWorld.ToSimpleMath());
    DirectX::BoundingSphere::CreateFromBoundingBox(ActorProbe.mWorldSphereBounds, ActorProbe.mWorldOBB);
    DirectX::BoundingBox::CreateFromSphere(ActorProbe.mWorldAABB, ActorProbe.mWorldSphereBounds);

    FSceneRenderData Scene{};
    Scene.mSceneId = mRenderSceneId;
    Scene.mObjectUpdates.push_back(FRenderObjectUpdate{FObjectHandle{0, 1}, ActorProbe, false});

    FLightProbe LightProbe{};
    LightProbe.mType = ELightType::Directional;
    LightProbe.mDirection = FVector{-0.5f, -0.5f, -1.0f};
    LightProbe.mColor = FVector{1.0f, 1.0f, 1.0f};
    LightProbe.mIntensity = 1.0f;

    Scene.mLightProbes.push_back(LightProbe);

    FSceneRenderSurface* Surface{PreviewSurface};
    if (Surface == nullptr) {
        const Uint64 ThumbnailKey{MakeThumbnailKey(Entry.mHandle)};
        FThumbnail& Thumbnail{mThumbnails[ThumbnailKey]};
        if (Thumbnail.mSurface == nullptr) {
            Thumbnail.mSurface = std::make_unique<FSceneRenderSurface>();
        }
        Thumbnail.mSurface->InitializeOffscreen(mRenderer->GetDevice(), ThumbnailSize, ThumbnailSize);
        if (!Thumbnail.mSurface->IsValid()) {
            Thumbnail.mSurface.reset();
            return;
        }
        Surface = Thumbnail.mSurface.get();
    }

    FRenderSettings RenderSettings{};
    RenderSettings.mClearColor = FVector4{ 0.075f, 0.080f, 0.095f, 1.0f};
    RenderSettings.mBRenderSky = false;

    FRenderView View{};
    View.mTarget = Surface;
    View.mCamera = BuildCamera();
    View.mUseLOD = true;
    View.mSettings = RenderSettings;
    View.mPasses.reset();
    View.SetPassEnabled(ERenderPass::SceneGeometry, true);
    Scene.mRevision = ++mRenderSceneRevision;
    mRenderer->RenderView(View, Scene);
}

ID3D11ShaderResourceView* FAssetThumbnailRenderer::GetThumbnail(FAssetHandle AssetHandle) const {
    const auto It{mThumbnails.find(MakeThumbnailKey(AssetHandle))};

    if (It == mThumbnails.end() || It->second.mSurface == nullptr) {
        return nullptr;
    }

    return It->second.mSurface->GetShaderResourceView();
}

void FAssetThumbnailRenderer::Terminate() {
    for (auto& [Key, Thumbnail] : mThumbnails) {
        if (Thumbnail.mSurface != nullptr) {
            Thumbnail.mSurface->Reset();
        }
    }

    mThumbnails.clear();
    mPendingAssetHandles.clear();
    mPendingAssetIndex = 0;

    mRenderer = nullptr;
    mAssetRegistry = nullptr;
}

FMatrix FAssetThumbnailRenderer::BuildMeshTransform(const UMesh& Mesh) const {
    const std::span<const FVector> Positions{Mesh.GetVertexAttributeData<EVertexAttribute::Position>()};

    if (Positions.empty()) {
        return FMatrix::Identity;
    }

    FVector BoundsMin{Positions.front()};
    FVector BoundsMax{Positions.front()};

    for (const FVector& Position : Positions) {
        BoundsMin = FVector::Min(BoundsMin, Position);
        BoundsMax = FVector::Max(BoundsMax, Position);
    }

    const FVector Center{(BoundsMin + BoundsMax) * 0.5f};
    const FVector Size{BoundsMax - BoundsMin};

    const float LargestExtent{std::max({Size.mX, Size.mY, Size.mZ})};

    if (LargestExtent <= 0.0001f) {
        return FMatrix::Identity;
    }

    const float UniformScale{1.6f / LargestExtent};

    return FMatrix::CreateTranslation(-Center) * FMatrix::CreateScale(UniformScale, UniformScale, UniformScale);
}

CameraProbe FAssetThumbnailRenderer::BuildCamera() const {
    const FVector Target{0.0f, 0.0f, 0.0f};
    const FVector Eye{3.0f, -3.0f, 2.25f};

    CameraProbe Camera{};
    Camera.mView = MakeCameraWorldMatrix(Eye, Target).Invert();
    Camera.mProjection = FMatrix::CreatePerspectiveFieldOfView(0.610865f, 1.0f, 0.1f, 100.0f);

    Camera.mViewProjection = Camera.mView * Camera.mProjection;

    FFrustum LocalFrustum{};
    FFrustum::CreateFromMatrix(LocalFrustum, Camera.mProjection.ToSimpleMath());
    LocalFrustum.Transform(Camera.mViewFrustum, Camera.mView.Inverse().ToSimpleMath());

    return Camera;
}

FMatrix FAssetThumbnailRenderer::MakeCameraWorldMatrix(const FVector& Eye, const FVector& Target) const {
    FVector Forward{Target - Eye};
    Forward.Normalize();

    FVector Right{FVector::UnitZ.Cross(Forward)};
    Right.Normalize();

    FVector Up{Forward.Cross(Right)};
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

const FAssetEntry* FAssetThumbnailRenderer::FindAssetEntry(FAssetHandle AssetHandle) const {
    if (mAssetRegistry == nullptr) {
        return nullptr;
    }

    for (const FAssetEntry& Entry : mAssetRegistry->GetAssetEntries()) {
        if (Entry.mHandle == AssetHandle) {
            return &Entry;
        }
    }

    return nullptr;
}

Uint64 FAssetThumbnailRenderer::MakeThumbnailKey(FAssetHandle AssetHandle) {
    return (static_cast<Uint64>(AssetHandle.mGeneration) << 32) | static_cast<Uint64>(AssetHandle.mId);
}
