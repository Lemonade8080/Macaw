#pragma once

#include "UPrimitiveComponent.h"
#include "Core/Asset/FAssetPath.h"
#include <array>
#include "Math/FMath.h"

class UBillboardComponent : public UPrimitiveComponent {
public:
    UBillboardComponent() = default;
    ~UBillboardComponent() override = default;

    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UBillboardComponent, UPrimitiveComponent);

    void SetTextureHandle(FAssetHandle InTextureHandle);
    void SetPipelineHandle(FAssetHandle InPipelineHandle);
    void SetSize(const FVector2& InSize);
    void SetUV(const FVector2& InUVMin, const FVector2& InUVMax);
    void SetColor(const FVector4& InColor);

    FAssetHandle GetTextureHandle() const;
    FAssetHandle GetPipelineHandle() const;
    const FVector2& GetSize() const;
    const FVector2& GetUVmin() const;
    const FVector2& GetUVMax() const;
    const FVector4& GetColor() const;

    bool MakeBillboardRender(FBillboardProbe& OutProbe) const;
    bool GetWorldCorners(const FMatrix& CameraWorld, std::array<FVector3, 4>& OutCorners) const;
    void DrawPanels(IPropertyEditorContext* Context) override;

    void OnRegister() override;
    void OnUnregister() override;

protected:

    bool CanRenderBillBoard() const;

    void Serialize(FArchive& Archive) override;

    virtual bool TryGetBillBoardWorld(FMatrix& OutWorld) const;

private:
    FAssetHandle mTextureHandle{};
    FAssetHandle mPipelineHandle{};
    FAssetPath mTextureAssetPath{};
    FAssetPath mPipelineAssetPath{};
    FGuid mTextureAssetGuid{};
    FGuid mPipelineAssetGuid{};

    FVector2 mSize{1.0f, 1.0f};
    FVector2 mUvMin{0.0f, 0.0f};
    FVector2 mUvMax{1.0f, 1.0f};
    FVector4 mColor{1.0f, 1.0f, 1.0f, 1.0f};
};
