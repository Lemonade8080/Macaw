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

# if 0
bool UMeshComponent::RaycastMesh(const FRay& Ray, float& OutDistance) const {
    const UMesh* Mesh{ResolveMesh()};
    if (Mesh == nullptr) {
        return false;
    }

    const auto Positions{Mesh->GetVertexAttributeData<EVertexAttribute::Position>()};
    const TArray<Uint32>& Indices{Mesh->GetIndices()};
    if (Positions.empty() || Indices.size() < 3) {
        return false;
    }

    bool BHit{false};
    float ClosestDistance{std::numeric_limits<float>::max()};
    const FMatrix WorldMatrix{GetComponentToWorld()};
    for (std::size_t Index{0}; Index + 2 < Indices.size(); Index += 3) {
        const Uint32 I0{Indices[Index]};
        const Uint32 I1{Indices[Index + 1]};
        const Uint32 I2{Indices[Index + 2]};
        if (I0 >= Positions.size() || I1 >= Positions.size() || I2 >= Positions.size()) {
            continue;
        }

        const DirectX::XMVECTOR V0{DirectX::XMVector3TransformCoord(Positions[I0].ToSimpleMath(), WorldMatrix.ToSimpleMath())};
        const DirectX::XMVECTOR V1{DirectX::XMVector3TransformCoord(Positions[I1].ToSimpleMath(), WorldMatrix.ToSimpleMath())};
        const DirectX::XMVECTOR V2{DirectX::XMVector3TransformCoord(Positions[I2].ToSimpleMath(), WorldMatrix.ToSimpleMath())};
        float Distance{0.0f};
        if (DirectX::TriangleTests::Intersects(Ray.position, Ray.direction, V0, V1, V2, Distance) && Distance < ClosestDistance) {
            ClosestDistance = Distance;
            BHit = true;
        }
    }

    if (BHit) {
        OutDistance = ClosestDistance;
    }
    return BHit;
}
#else
bool UMeshComponent::RaycastMesh(const FRay& Ray, float& OutDistance) const {
    const UMesh* Mesh{ ResolveMesh() };
    if (Mesh == nullptr) {
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
    float ClosestDistance{ 0.0f };
    const bool BHit = Mesh->Raycast(LocalRay, ClosestDistance);

    if (BHit) {
        OutDistance = static_cast<float>(ClosestDistance / LocalDirectionLength);
    }
    return BHit;
}
#endif


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
