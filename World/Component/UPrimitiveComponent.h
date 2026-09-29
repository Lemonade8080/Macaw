#pragma once

#include "UActorComponent.h"
#include "USceneComponent.h"

#include "../../Core/Base/FRenderProbe.h"

class UPrimitiveComponent : public USceneComponent {
public:
    UPrimitiveComponent() = default;
    ~UPrimitiveComponent() override = default;

    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UPrimitiveComponent, USceneComponent)

    bool IsVisible() const;
    void SetVisible(bool BInVisible);
    void DrawPanels(IPropertyEditorContext* Context) override;
    void OnRegister() override;
    void OnUnregister() override;

    virtual void MakeRender(FActorProbe& OutProbe) const;

    void SetPickingBox(const DirectX::BoundingOrientedBox& Box);

    const DirectX::BoundingOrientedBox& GetPickingBox() const;

    void BuildBoundingSphereFromOBB();
    const DirectX::BoundingSphere& GetBoundingSphere() const;

protected:
    void Serialize(FArchive& Archive) override;

private:
    bool mBVisible{true};
    DirectX::BoundingOrientedBox mPickingBox{DirectX::XMFLOAT3{0.f, 0.f, 0.f}, DirectX::XMFLOAT3{0.f, 0.f, 0.f}, DirectX::XMFLOAT4{0.f, 0.f, 0.f, 1.f}};
    DirectX::BoundingSphere mBoundingSphere{};
};
