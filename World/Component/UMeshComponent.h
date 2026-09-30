#pragma once

#include "UPrimitiveComponent.h"
#include "FMeshPickingProxy.h"
#include "Core/Base/FAssetHandle.h"
#include "Core/Asset/FAssetPath.h"
#include "Core/Base/FGuid.h"
#include "Asset/UMesh.h"

class UMeshComponent : public UPrimitiveComponent {
public:
    UMeshComponent() = default;
    ~UMeshComponent() override = default;

public:
    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UMeshComponent, UPrimitiveComponent)

    FAssetHandle GetMeshHandle() const;
    virtual void SetMeshHandle(FAssetHandle InHandle);
    void DrawPanels(IPropertyEditorContext* Context) override;
    void OnRegister() override;

    virtual const UMesh* ResolveMesh() const;
    bool BuildPickingBoxFromMesh();
    bool RaycastMesh(const FRay& Ray, float& OutDistance, float MaxDistance = std::numeric_limits<float>::max()) const;

protected:
    void Serialize(FArchive& Archive) override;

private:
    mutable FMeshPickingProxy mRaycastProxy;
    mutable Uint64 mRaycastTransformRevision = 0;
    mutable bool mRaycastTransformInitialized = false;
    FAssetHandle mMeshHandle{};
    FAssetPath mMeshAssetPath{};
    FGuid mMeshAssetGuid{};
};
