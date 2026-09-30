#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UMeshComponent.h"

#include "World/AActor.h"
#include "World/UWorld.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMesh.h"

FAssetHandle UMeshComponent::GetMeshHandle() const {
    return mMeshHandle;
}

void UMeshComponent::SetMeshHandle(FAssetHandle InHandle) {
    mMeshHandle = InHandle;
    AActor* Owner{GetOwner()};
    UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mMeshAssetPath = Registry != nullptr && Registry->GetAssetPath(mMeshHandle) != nullptr ? *Registry->GetAssetPath(mMeshHandle) : FAssetPath{};
    mMeshAssetGuid = Registry != nullptr && Registry->GetAssetGuid(mMeshHandle) != nullptr ? *Registry->GetAssetGuid(mMeshHandle) : FGuid{};
    BuildPickingBoxFromMesh();
    OnRenderStateChanged();
}

const UMesh* UMeshComponent::ResolveMesh() const {
    AActor* Owner{GetOwner()};
    UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    return Registry != nullptr ? Registry->ResolveAsset<UMesh>(mMeshHandle) : nullptr;
}

void UMeshComponent::OnRegister() {
    UPrimitiveComponent::OnRegister();
    BuildPickingBoxFromMesh();
}

bool UMeshComponent::BuildPickingBoxFromMesh() {
    const UMesh* Mesh{ResolveMesh()};
    if (Mesh == nullptr) {
        return false;
    }

    SetPickingBox(Mesh->GetBoundingBox());
    return true;
}

const UMesh* FMeshPickingProxy::GetMesh() const { return Source != nullptr ? Source->Mesh : nullptr; }

void FMeshPickingProxy::Update(const UMesh* InMesh, const FTransform& Transform) {
    Source = InMesh != nullptr ? InMesh->GetPickingSource() : nullptr;
    const FVector3& Scale = Transform.GetScale();
    Valid = std::isfinite(Scale.mX) && std::isfinite(Scale.mY) && std::isfinite(Scale.mZ) && Scale.mX != 0.0f && Scale.mY != 0.0f && Scale.mZ != 0.0f;
    if (!Valid) return;
    ReverseWinding = (Scale.mX < 0.0f) ^ (Scale.mY < 0.0f) ^ (Scale.mZ < 0.0f);
    InverseScale = {1.0f / Scale.mX, 1.0f / Scale.mY, 1.0f / Scale.mZ};
    DirectX::XMStoreFloat3(&Position, Transform.GetPosition().ToSimpleMath());
    DirectX::XMStoreFloat4(&Rotation, DirectX::XMQuaternionNormalize(Transform.GetRotationQuaternion().ToSimpleMath()));
}

bool UMeshComponent::RaycastMesh(const FRay& Ray, float& OutDistance, float MaxDistance) const {
    const UMesh* Mesh = ResolveMesh();
    const Uint64 Revision = GetTransformRevision();
    if (!mRaycastTransformInitialized || mRaycastTransformRevision != Revision || mRaycastProxy.GetMesh() != Mesh) {
        mRaycastProxy.Update(Mesh, GetComponentTransform());
        mRaycastTransformRevision = Revision;
        mRaycastTransformInitialized = true;
    }
    return mRaycastProxy.Raycast(Ray, OutDistance, MaxDistance);
}

bool FMeshPickingProxy::PrepareRay(const FRay& Ray, FRay& LocalRay, double& DirectionLength) const {
    if (!Valid) return false;
    const auto Scale = DirectX::XMLoadFloat3(&InverseScale), Q = DirectX::XMLoadFloat4(&Rotation);
    const auto Origin = DirectX::XMVectorMultiply(DirectX::XMVector3InverseRotate(DirectX::XMVectorSubtract(Ray.position, DirectX::XMLoadFloat3(&Position)), Q), Scale);
    const auto Direction = DirectX::XMVectorMultiply(DirectX::XMVector3InverseRotate(Ray.direction, Q), Scale);
    const double DX = DirectX::XMVectorGetX(Direction), DY = DirectX::XMVectorGetY(Direction), DZ = DirectX::XMVectorGetZ(Direction);
    DirectionLength = std::sqrt(DX * DX + DY * DY + DZ * DZ);
    if (!std::isfinite(DirectionLength) || DirectionLength <= 0.0 || DirectX::XMVector3IsNaN(Origin) || DirectX::XMVector3IsInfinite(Origin)) return false;
    LocalRay = FRay{Origin, DirectX::XMVectorSet(static_cast<float>(DX / DirectionLength), static_cast<float>(DY / DirectionLength), static_cast<float>(DZ / DirectionLength), 0.0f)};
    return true;
}

bool FMeshPickingProxy::RaycastPrepared(const FRay& LocalRay, double DirectionLength, float& OutDistance, float MaxDistance) const {
    if (Source == nullptr || !(MaxDistance >= 0.0f)) return false;
    const double Limit = static_cast<double>(MaxDistance) * DirectionLength;
    const float LocalLimit = Limit >= std::numeric_limits<float>::max() ? std::numeric_limits<float>::max() : std::bit_cast<float>(std::bit_cast<Uint32>((std::max)(0.0f, static_cast<float>(Limit))) + 1u);
    float Distance = 0.0f;
    if (!Source->Raycast(LocalRay, Distance, LocalLimit, ReverseWinding)) return false;
    const float WorldDistance = static_cast<float>(Distance / DirectionLength);
    if (WorldDistance > MaxDistance) return false;
    OutDistance = WorldDistance;
    return true;
}

bool FMeshPickingProxy::Raycast(const FRay& Ray, float& OutDistance, float MaxDistance) const {
    FRay LocalRay; double DirectionLength;
    return PrepareRay(Ray, LocalRay, DirectionLength) && RaycastPrepared(LocalRay, DirectionLength, OutDistance, MaxDistance);
}

void UMeshComponent::Serialize(FArchive& Archive) {
    UPrimitiveComponent::Serialize(Archive);

    const IAssetRegistry* Registry{Archive.GetAssetRegistry()};
    if (Archive.IsSaving() && Registry != nullptr) {
        if (const FAssetPath* AssetPath{Registry->GetAssetPath(mMeshHandle)}) {
            mMeshAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{Registry->GetAssetGuid(mMeshHandle)}) {
            mMeshAssetGuid = *AssetGuid;
        }
    }

    Archive.Serialize("MeshAssetGuid", mMeshAssetGuid);
    Archive.Serialize("MeshAssetPath", mMeshAssetPath.mPath);
    if (Archive.IsLoading()) {
        mMeshHandle = Registry != nullptr ? Registry->FindAsset(mMeshAssetGuid) : FAssetHandle{};
        if (!mMeshHandle && Registry != nullptr) {
            mMeshHandle = Registry->FindAsset(mMeshAssetPath);
        }
        BuildPickingBoxFromMesh();
        OnRenderStateChanged();
    }
}

void UMeshComponent::DrawPanels(IPropertyEditorContext* Context) {
    UPrimitiveComponent::DrawPanels(Context);
    Context->DrawAssetPicker("Mesh", *UMesh::StaticTypeInfo(), GetMeshHandle(), [this](FAssetHandle Handle) {
        SetMeshHandle(Handle);
    });
}
