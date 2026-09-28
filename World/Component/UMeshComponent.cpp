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
