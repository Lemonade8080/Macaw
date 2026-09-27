#pragma once

#include "UPrimitiveComponent.h"
#include "Core/Base/TypeInfo.h"
#include "Core/Archive/FArchive.h"
#include "Core/Render/ILineDrawContext.h"
#include "World/Component/UMeshComponent.h"

class UCollisionComponent : public UPrimitiveComponent {
public:
    UCollisionComponent() = default;
    ~UCollisionComponent() override = default;

public:
    void OnRegister() override;
    void OnUnregister() override;

    bool IsCollisionEnabled() const;
    void SetCollisionEnabled(bool BEnabled);
    void DrawPanels(IPropertyEditorContext* Context) override;

    bool Raycast(const FRay& Ray, float& OutDistance) const;
    virtual void DrawEditorBounds(ILineDrawContext* LineContext, ELineDepthMode DepthMode) const = 0;

    void MakeRender(FActorProbe& OutProbe) const override;

    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UCollisionComponent, UPrimitiveComponent)

    virtual bool RaycastBounds(const FRay& Ray, float& OutDistance) const = 0;

    virtual UMeshComponent* GetMeshComponent() const;

protected:
    void Serialize(FArchive& Archive) override;

private:
    bool mBCollisionEnabled{true};
};
