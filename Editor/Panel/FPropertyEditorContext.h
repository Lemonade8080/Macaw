#pragma once

#include "Core/Property/IPropertyEditorContext.h"
#include "ImGui/imgui.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Editor/View/FAssetThumbnailRenderer.h"
#include <d3d11.h>

class FPropertyEditorContext final : public IPropertyEditorContext {
public:
    FPropertyEditorContext() = default;
    ~FPropertyEditorContext() override = default;

public:
    bool BeginCategory(const char* Label, bool BDefaultOpen = true) const override;
    void DrawDisabledText(const char* Text) const override;
    void DrawButton(const char* Label, const std::function<void()>& OnClicked) const override;

    void DrawBool(const char* Label, bool Value, const std::function<void(bool)>& Setter) const override;
    void DrawFloat(const char* Label, float Value, float Speed, float Min, float Max, const std::function<void(float)>& Setter) const override;
    void DrawVector2(const char* Label, const FVector2& Value, float Speed, float Min, float Max, const std::function<void(const FVector2&)>& Setter) const override;
    void DrawVector3(const char* Label, const FVector3& Value, float Speed, float Min, float Max, const std::function<void(const FVector3&)>& Setter) const override;
    void DrawColor(const char* Label, const FVector4& Value, const std::function<void(const FVector4&)>& Setter) const override;
    void DrawText(const char* Label, const FString& Value, const std::function<void(const FString&)>& Setter) const override;
    void DrawTransform(const char* Label, const FTransform& Value, const std::function<void(const FTransform&)>& Setter) override;

    void DrawReferencePicker(const char* Label, const char* Preview, bool BNoneSelected, const std::function<void()>& ClearSelection, const std::vector<FPropertyReferenceOption>& Options) const override;
    void DrawAssetPicker(const char* Label, const FTypeInfo& AssetType, FAssetHandle CurrentHandle, const std::function<void(FAssetHandle)>& Setter) const override;

    void BindAssetRegistry(const IAssetRegistry* InAssetRegistry);
    void BindThumbnailRenderer(FAssetThumbnailRenderer* InThumbnailRenderer);

private:
    void UpdateTransformFields(const FTransform& Transform);
    FTransform BuildDesiredTransform() const;

    bool SupportsAssetThumbnail(const FTypeInfo& AssetType) const;

    ID3D11ShaderResourceView* GetAssetThumbnail(const IAssetRegistry* Registry, FAssetHandle AssetHandle) const;

    bool DrawAssetOption(const char* Label, ID3D11ShaderResourceView* Thumbnail, bool BSelected) const;

private:
    const IAssetRegistry* mAssetRegistry{nullptr};
    FAssetThumbnailRenderer* mThumbnailRenderer{nullptr};

    ImGuiID mEditingTransformId{0};
    FVector3 mEditPosition{};
    FRotator mEditRotation{};
    FVector3 mEditScale{1.0f, 1.0f, 1.0f};
    bool mBAbsoluteLocation{false};
    bool mBAbsoluteRotation{false};
    bool mBAbsoluteScale{false};
};
