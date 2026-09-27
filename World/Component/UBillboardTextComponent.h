#pragma once

#include "UPrimitiveComponent.h"

#include "Core/Base/FAssetHandle.h"
#include "Core/Asset/FAssetPath.h"
#include "Core/Base/FRenderProbe.h"
#include "Core/STL.h"
#include "Asset/UFont.h"
#include "Core/Property/IPropertyEditorContext.h"

class UBillboardTextComponent : public UPrimitiveComponent {
public:
    UBillboardTextComponent() = default;
    ~UBillboardTextComponent() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(UBillboardTextComponent, UPrimitiveComponent);

    void SetFontHandle(FAssetHandle InFontHandle);
    void SetPipelineHandle(FAssetHandle InPipelineHandle);

    void SetText(const FString& InText);
    void SetColor(const FVector4& InColor);

    void SetCharacterHeight(float InCharacterHeight);
    void SetLetterSpacing(float InLetterSpacing);
    void SetLineSpacing(float InLineSpacing);

    FAssetHandle GetFontHandle() const;
    FAssetHandle GetPipelineHandle() const;

    const FString& GetText() const;
    const FVector4& GetColor() const;

    float GetCharacterHeight() const;
    float GetLetterSpacing() const;
    float GetLineSpacing() const;

    const TArray<FTextVertex>& GetVertices() const;
    virtual bool MakeTextRender(FTextProbe& OutProbe) const;

    void OnRegister() override;
    void OnUnregister() override;

    void DrawPanels(IPropertyEditorContext* Context) override;

protected:
    virtual bool TryGetTextWorld(FMatrix& OutWorld) const;
    void Serialize(FArchive& Archive) override;

    void RebuildTextGeometry();

protected:
    FAssetHandle mFontHandle{};
    FAssetHandle mPipelineHandle{};
    FAssetPath mFontAssetPath{};
    FAssetPath mPipelineAssetPath{};
    FGuid mFontAssetGuid{};
    FGuid mPipelineAssetGuid{};

    FString mText{};

    FVector4 mColor{1.0f, 1.0f, 1.0f, 1.0f};

    float mCharacterHeight{1.0f};
    float mLetterSpacing{0.0f};
    float mLineSpacing{0.0f};

    TArray<FTextVertex> mVertices{};
};
