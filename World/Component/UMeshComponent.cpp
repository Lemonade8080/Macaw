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

bool UMeshComponent::RaycastMesh(const FRay& Ray, float& OutDistance, float MaxDistance) const {
    const UMesh* Mesh{ ResolveMesh() };
    if (Mesh == nullptr || !(MaxDistance >= 0.0f)) {
        return false;
    }

    // Component transforms are composed as SRT, without hierarchy-induced shear.
    const FTransform WorldTransform{ GetComponentTransform() };
    const FVector3& Scale{ WorldTransform.GetScale() };
    if (!std::isfinite(Scale.mX) || !std::isfinite(Scale.mY) || !std::isfinite(Scale.mZ) ||
        Scale.mX == 0.0f || Scale.mY == 0.0f || Scale.mZ == 0.0f) {
        return false;
    }

    const DirectX::XMVECTOR InverseScale{ DirectX::XMVectorSet(1.0f / Scale.mX, 1.0f / Scale.mY, 1.0f / Scale.mZ, 0.0f) };
    const DirectX::XMVECTOR Rotation{ DirectX::XMQuaternionNormalize(WorldTransform.GetRotationQuaternion().ToSimpleMath()) };
    const DirectX::XMVECTOR Offset{ DirectX::XMVectorSubtract(Ray.position, WorldTransform.GetPosition().ToSimpleMath()) };
    const DirectX::XMVECTOR LocalOrigin{ DirectX::XMVectorMultiply(DirectX::XMVector3InverseRotate(Offset, Rotation), InverseScale) };
    const DirectX::XMVECTOR UnnormalizedLocalDirection{ DirectX::XMVectorMultiply(DirectX::XMVector3InverseRotate(Ray.direction, Rotation), InverseScale) };
    const double DirectionX{ DirectX::XMVectorGetX(UnnormalizedLocalDirection) };
    const double DirectionY{ DirectX::XMVectorGetY(UnnormalizedLocalDirection) };
    const double DirectionZ{ DirectX::XMVectorGetZ(UnnormalizedLocalDirection) };
    const double LocalDirectionLength{ std::hypot(DirectionX, DirectionY, DirectionZ) };
    if (!std::isfinite(LocalDirectionLength) || LocalDirectionLength <= 0.0f ||
        DirectX::XMVector3IsNaN(LocalOrigin) || DirectX::XMVector3IsInfinite(LocalOrigin)) {
        return false;
    }
    const DirectX::XMVECTOR LocalDirection{ DirectX::XMVectorSet(
        static_cast<float>(DirectionX / LocalDirectionLength),
        static_cast<float>(DirectionY / LocalDirectionLength),
        static_cast<float>(DirectionZ / LocalDirectionLength), 0.0f) };

    const FRay LocalRay{ LocalOrigin, LocalDirection };
    const double LocalLimit = static_cast<double>(MaxDistance) * LocalDirectionLength;
    const float LocalMaxDistance = LocalLimit >= std::numeric_limits<float>::max() ? std::numeric_limits<float>::max() : std::nextafter(static_cast<float>(LocalLimit), std::numeric_limits<float>::infinity());
    float ClosestDistance{ 0.0f };
    const bool BHit = Mesh->Raycast(LocalRay, ClosestDistance, LocalMaxDistance);

    if (BHit) {
        const float WorldDistance = static_cast<float>(ClosestDistance / LocalDirectionLength);
        if (WorldDistance > MaxDistance) return false;
        OutDistance = WorldDistance;
    }
    return BHit;
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
    }
}

void UMeshComponent::DrawPanels(IPropertyEditorContext* Context) {
    UPrimitiveComponent::DrawPanels(Context);
    Context->DrawAssetPicker("Mesh", *UMesh::StaticTypeInfo(), GetMeshHandle(), [this](FAssetHandle Handle) {
        SetMeshHandle(Handle);
    });
}
