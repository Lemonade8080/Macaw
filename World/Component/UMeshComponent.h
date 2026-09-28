#pragma once

#include "UPrimitiveComponent.h"
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
    bool RaycastMesh(const FRay& Ray, float& OutDistance) const;

protected:
    void Serialize(FArchive& Archive) override;

private:
    FAssetHandle mMeshHandle{};
    FAssetPath mMeshAssetPath{};
    FGuid mMeshAssetGuid{};
};
