#pragma once

#include "UMeshComponent.h"
#include "Core/Archive/FArchive.h"

class UStaticMeshComponent : public UMeshComponent {
public:
    UStaticMeshComponent() = default;
    ~UStaticMeshComponent() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(UStaticMeshComponent, UMeshComponent)

    FAssetHandle GetMaterialHandle() const;
    FAssetHandle GetPipelineHandle() const;

    void SetMaterialHandle(FAssetHandle InHandle);
    void SetPipelineHandle(FAssetHandle InHandle);
    void DrawPanels(IPropertyEditorContext* Context) override;

    void OnRegister() override;
    void OnUnregister() override;
    virtual void MakeRender(FActorProbe& OutProbe) const override;

protected:
    void Serialize(FArchive& Archive) override;

private:
    void EnsureDefaultRenderAssets();

    FAssetHandle mMaterialHandle{};
    FAssetHandle mPipelineHandle{};
    FAssetPath mMaterialAssetPath{};
    FAssetPath mPipelineAssetPath{};
    FGuid mMaterialAssetGuid{};
    FGuid mPipelineAssetGuid{};
};
