#pragma once

#include "UCollisionComponent.h"
#include "World/Component/UMeshComponent.h"

class UBoxColliderComponent final : public UCollisionComponent {
public:
    UBoxColliderComponent() = default;
    ~UBoxColliderComponent() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UBoxColliderComponent, UCollisionComponent)

    void SetMeshComponent(UMeshComponent* InMeshComponent);
    UMeshComponent* GetMeshComponent() const override;
    bool BuildBoundsFromMesh();

    bool RaycastBounds(const FRay& Ray, float& OutDistance) const override;
    void DrawEditorBounds(ILineDrawContext* LineContext, ELineDepthMode DepthMode) const override;
    FVector3 GetExtent() const;
    void SetExtent(const FVector3& InExtent);
    void DrawPanels(IPropertyEditorContext* Context) override;

    bool ResolveLoadedReferences() override;
    void InitializeComponent() override;

protected:
    void Serialize(FArchive& Archive) override;

private:
    TObjectRef<UMeshComponent> mMeshComponent{};
    FGuid mPendingMeshComponentGuid{};
    DirectX::BoundingOrientedBox mObb{DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f}, DirectX::XMFLOAT3{1.0f, 1.0f, 1.0f}, DirectX::XMFLOAT4{0.0f, 0.0f, 0.0f, 1.0f}};
};
