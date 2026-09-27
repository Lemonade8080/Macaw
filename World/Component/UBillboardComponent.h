#pragma once

#include "UPrimitiveComponent.h"
#include "Core/Asset/FAssetPath.h"
#include <array>
#include "Math/FMath.h"

// 카메라를 향하는 Primitive의 공통 기반 클래스.
// Billboard의 실제 방향 계산은 Shader에서 수행한다.
// 이 클래스는 렌더링 여부와 Billboard 원점만 제공한다.
class UBillboardComponent : public UPrimitiveComponent {
public:
    UBillboardComponent() = default;
    ~UBillboardComponent() override = default;

    JG_DECLARE_ABSTRACT_DERIVED_TYPEINFO(UBillboardComponent, UPrimitiveComponent);

    // Sprite
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

    //Billboard를 현재 프레임에 렌더할 수 있는지 검사한다.
    bool CanRenderBillBoard() const;

    void Serialize(FArchive& Archive) override;

    // Billboard 렌더링에 사용할 World Transform을 반환한다.
    // 기본 구현 : 자신의 ComponentToWorld 사용
    // UNameTagComponent: Target Actor Transform + Offset 사용
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
