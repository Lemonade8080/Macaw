#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UMeshComponent.h"
#include "Core/Spatial/FPickingMath.h"

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
    Valid = InMesh != nullptr && std::isfinite(Scale.mX) && std::isfinite(Scale.mY) && std::isfinite(Scale.mZ) && Scale.mX != 0.0f && Scale.mY != 0.0f && Scale.mZ != 0.0f;
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

template<bool Scalar> static bool RaycastMeshProxy(const FMeshPickingProxy& Proxy, const FRay& Ray, float& OutDistance, float MaxDistance) {
    const UMesh* Mesh = Proxy.GetMesh();
    if (!Proxy.Valid || Mesh == nullptr || !(MaxDistance >= 0.0f)) return false;
    DirectX::XMFLOAT3 LocalOrigin, LocalDirectionValue;
    if constexpr (Scalar) {
        LocalOrigin = PickingMath::InverseRotateScalar({PickingMath::ScalarSubtract(Ray.position.x, Proxy.Position.x), PickingMath::ScalarSubtract(Ray.position.y, Proxy.Position.y), PickingMath::ScalarSubtract(Ray.position.z, Proxy.Position.z)}, Proxy.Rotation);
        LocalDirectionValue = PickingMath::InverseRotateScalar({Ray.direction.x, Ray.direction.y, Ray.direction.z}, Proxy.Rotation);
        LocalOrigin.x = PickingMath::ScalarMultiply(LocalOrigin.x, Proxy.InverseScale.x); LocalOrigin.y = PickingMath::ScalarMultiply(LocalOrigin.y, Proxy.InverseScale.y); LocalOrigin.z = PickingMath::ScalarMultiply(LocalOrigin.z, Proxy.InverseScale.z);
        LocalDirectionValue.x = PickingMath::ScalarMultiply(LocalDirectionValue.x, Proxy.InverseScale.x); LocalDirectionValue.y = PickingMath::ScalarMultiply(LocalDirectionValue.y, Proxy.InverseScale.y); LocalDirectionValue.z = PickingMath::ScalarMultiply(LocalDirectionValue.z, Proxy.InverseScale.z);
    } else {
        const auto Scale = DirectX::XMLoadFloat3(&Proxy.InverseScale), Rotation = DirectX::XMLoadFloat4(&Proxy.Rotation);
        const auto Offset = DirectX::XMVectorSubtract(Ray.position, DirectX::XMLoadFloat3(&Proxy.Position));
        DirectX::XMStoreFloat3(&LocalOrigin, DirectX::XMVectorMultiply(DirectX::XMVector3InverseRotate(Offset, Rotation), Scale));
        DirectX::XMStoreFloat3(&LocalDirectionValue, DirectX::XMVectorMultiply(DirectX::XMVector3InverseRotate(Ray.direction, Rotation), Scale));
    }
    const double DirectionX = LocalDirectionValue.x, DirectionY = LocalDirectionValue.y, DirectionZ = LocalDirectionValue.z;
    const double LocalDirectionLength{ std::hypot(DirectionX, DirectionY, DirectionZ) };
    if (!std::isfinite(LocalDirectionLength) || LocalDirectionLength <= 0.0f ||
        !std::isfinite(LocalOrigin.x) || !std::isfinite(LocalOrigin.y) || !std::isfinite(LocalOrigin.z)) {
        return false;
    }
    const FRay LocalRay{{LocalOrigin.x, LocalOrigin.y, LocalOrigin.z}, {static_cast<float>(DirectionX / LocalDirectionLength), static_cast<float>(DirectionY / LocalDirectionLength), static_cast<float>(DirectionZ / LocalDirectionLength)}};
    const double LocalLimit = static_cast<double>(MaxDistance) * LocalDirectionLength;
    const float LocalMaxDistance = LocalLimit >= std::numeric_limits<float>::max() ? std::numeric_limits<float>::max() : std::nextafter(static_cast<float>(LocalLimit), std::numeric_limits<float>::infinity());
    float ClosestDistance{ 0.0f };
    const bool BHit = Mesh->Raycast(LocalRay, ClosestDistance, LocalMaxDistance, Proxy.ReverseWinding);

    if (BHit) {
        const float WorldDistance = static_cast<float>(ClosestDistance / LocalDirectionLength);
        if (WorldDistance > MaxDistance) return false;
        OutDistance = WorldDistance;
    }
    return BHit;
}

namespace { auto MeshProxyRaycaster = RaycastMeshProxy<false>; }
void FMeshPickingProxy::InitializeRaycast(bool Scalar) { MeshProxyRaycaster = Scalar ? RaycastMeshProxy<true> : RaycastMeshProxy<false>; }
bool FMeshPickingProxy::Raycast(const FRay& Ray, float& OutDistance, float MaxDistance) const { return MeshProxyRaycaster(*this, Ray, OutDistance, MaxDistance); }

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
