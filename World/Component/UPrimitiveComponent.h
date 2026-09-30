#pragma once

#include "UActorComponent.h"
#include "USceneComponent.h"

#include "../../Core/Base/FRenderProbe.h"

class UPrimitiveComponent : public USceneComponent {
public:
    UPrimitiveComponent() = default;
    ~UPrimitiveComponent() override = default;

public:
    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UPrimitiveComponent, USceneComponent)

    bool IsVisible() const;
    void SetVisible(bool BInVisible);

    void DrawPanels(IPropertyEditorContext* Context) override;

    void OnRegister() override;
    void OnUnregister() override;
    
    void UpdateBounds();

    virtual void MakeRender(FActorProbe& OutProbe) const;

    void SetPickingBox(const DirectX::BoundingOrientedBox& Box);

    const DirectX::BoundingOrientedBox& GetPickingBox() const;

    void BuildBoundsFromOBB();

    const DirectX::BoundingSphere& GetBoundingSphere() const;

    const DirectX::BoundingBox& GetWorldAABB() const;
    const DirectX::BoundingOrientedBox& GetWorldOBB() const;
    const DirectX::BoundingSphere& GetWorldSphere() const;

    virtual void OnTransformUpdate() override;

    void Serialize(FArchive& Archive) override;

private:
    bool mBVisible{true};

    DirectX::BoundingBox mLocalAABB{};
    DirectX::BoundingOrientedBox mPickingBox{DirectX::XMFLOAT3{0.f, 0.f, 0.f}, DirectX::XMFLOAT3{0.f, 0.f, 0.f}, DirectX::XMFLOAT4{0.f, 0.f, 0.f, 1.f}};
    DirectX::BoundingSphere mLocalSphere{};

    DirectX::BoundingBox mWorldAABB{};
    DirectX::BoundingOrientedBox mWorldOBB{};
    DirectX::BoundingSphere mWorldSphere{};

    Uint64 mWorldBoundsTransformRevision{};
    bool mWorldBoundsDirty{ true };
};
