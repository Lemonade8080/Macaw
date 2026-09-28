#include "pch.h"
#include "FMaterialEditorPanel.h"

#include "Asset/FAssetRegistry.h"
#include "Asset/USurfaceOpaque.h"
#include "Asset/UTexture.h"
#include "Editor/View/FAssetThumbnailRenderer.h"
#include "Render/Renderer.h"

#include <algorithm>
#include <array>

namespace {
    constexpr Uint32 PreviewSize{512};

    struct FTextureField {
        const char* mLabel{};
        FMaterialTextureMap FMaterialGroup::* mMember{};
    };

    constexpr std::array<FTextureField, 12> TextureFields{{{"Ambient", &FMaterialGroup::mAmbientTexture}, {"Diffuse", &FMaterialGroup::mDiffuseTexture}, {"Specular", &FMaterialGroup::mSpecularTexture}, {"Emissive", &FMaterialGroup::mEmissiveTexture}, {"Transmission", &FMaterialGroup::mTransmissionTexture}, {"Shininess", &FMaterialGroup::mShininessTexture}, {"Opacity", &FMaterialGroup::mOpacityTexture}, {"Bump", &FMaterialGroup::mBumpTexture}, {"Normal", &FMaterialGroup::mNormalTexture}, {"Displacement", &FMaterialGroup::mDisplacementTexture}, {"Decal", &FMaterialGroup::mDecalTexture}, {"Reflection", &FMaterialGroup::mReflectionTexture}}};
}

FMaterialEditorPanel::FMaterialEditorPanel(FAssetRegistry& InRegistry, FAssetThumbnailRenderer& InThumbnailRenderer)
    : FEditorWindow("Material Editor###MaterialEditor", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking),
      mRegistry(InRegistry),
      mThumbnailRenderer(InThumbnailRenderer) {
    SetVisible(false);
    mPropertyEditor.BindAssetRegistry(&InRegistry);
    mPropertyEditor.BindThumbnailRenderer(&InThumbnailRenderer);
}

FMaterialEditorPanel::~FMaterialEditorPanel() = default;

void FMaterialEditorPanel::OpenMaterial(FAssetHandle MaterialHandle) {
    if (mRegistry.ResolveAsset<USurfaceOpaque>(MaterialHandle) == nullptr) {
        return;
    }

    mMaterialHandle = MaterialHandle;
    mPreviewDirty = true;
    SetVisible(true);
}

void FMaterialEditorPanel::RenderOffscreen(FRenderer& Renderer, FAssetRegistry&) {
    if (!mPreviewDirty || mRegistry.ResolveAsset<USurfaceOpaque>(mMaterialHandle) == nullptr) {
        return;
    }

    if (!mPreviewSurface.IsValid()) {
        mPreviewSurface.InitializeOffscreen(Renderer.GetDevice(), PreviewSize, PreviewSize);
    }

    if (mPreviewSurface.IsValid()) {
        mThumbnailRenderer.RenderThumbnail(mMaterialHandle);
        mThumbnailRenderer.RenderMaterialPreview(mMaterialHandle, mPreviewSurface);
        mPreviewDirty = false;
    }
}

void FMaterialEditorPanel::ReleaseRenderResources() {
    mPreviewSurface.Reset();
    mPreviewDirty = true;
}

void FMaterialEditorPanel::DrawContents() {
    USurfaceOpaque* Material{mRegistry.ResolveAsset<USurfaceOpaque>(mMaterialHandle)};
    if (Material == nullptr) {
        ImGui::TextDisabled("Material unavailable.");
        return;
    }

    const FAssetPath* AssetPath{mRegistry.GetAssetPath(mMaterialHandle)};
    ImGui::TextUnformatted(AssetPath != nullptr ? AssetPath->mPath.c_str() : Material->GetAssetName().c_str());

    const float PreviewWidth{std::min(ImGui::GetContentRegionAvail().x, 400.0f)};
    if (ID3D11ShaderResourceView* Preview{mPreviewSurface.GetShaderResourceView()}; Preview != nullptr) {
        ImGui::Image(ImTextureRef{reinterpret_cast<ImTextureID>(Preview)}, ImVec2{PreviewWidth, PreviewWidth});
    } else {
        ImGui::Dummy(ImVec2{PreviewWidth, PreviewWidth});
    }

    ImGui::Separator();
    const TArray<FMaterialGroup>& Groups{Material->GetGroups()};
    for (Uint32 GroupIndex{}; GroupIndex < Groups.size(); ++GroupIndex) {
        ImGui::PushID(static_cast<int>(GroupIndex));
        const FString& GroupName{Groups[GroupIndex].mName};
        if (ImGui::CollapsingHeader(GroupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawGroup(*Material, GroupIndex, Groups[GroupIndex]);
        }
        ImGui::PopID();
    }
}

void FMaterialEditorPanel::DrawGroup(USurfaceOpaque& Material, Uint32 GroupIndex, const FMaterialGroup& Group) {
    const auto DrawColor3{[this, &Material, GroupIndex](const char* Label, const FVector3& Value, FVector3 FMaterialGroup::* Member) {
        const FVector4 Color{Value.mX, Value.mY, Value.mZ, 1.0f};
        mPropertyEditor.DrawColor(Label, Color, [this, &Material, GroupIndex, Member](const FVector4& Edited) {
            ModifyGroup(Material, GroupIndex, [Member, &Edited](FMaterialGroup& Target) {
                Target.*Member = FVector3{Edited.mX, Edited.mY, Edited.mZ};
            });
        });
    }};
    DrawColor3("Ambient", Group.mAmbient, &FMaterialGroup::mAmbient);
    DrawColor3("Diffuse", Group.mDiffuse, &FMaterialGroup::mDiffuse);
    DrawColor3("Specular", Group.mSpecular, &FMaterialGroup::mSpecular);
    DrawColor3("Emissive", Group.mEmissive, &FMaterialGroup::mEmissive);
    mPropertyEditor.DrawVector3("Transmission Filter", Group.mTransmissionFilter, 0.01f, 0.0f, 1.0f, [this, &Material, GroupIndex](const FVector3& Value) {
        ModifyGroup(Material, GroupIndex, [&Value](FMaterialGroup& Target) {
            Target.mTransmissionFilter = Value;
        });
    });
    mPropertyEditor.DrawFloat("Shininess", Group.mShininess, 1.0f, 0.0f, 1000.0f, [this, &Material, GroupIndex](float Value) {
        ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) {
            Target.mShininess = Value;
        });
    });
    mPropertyEditor.DrawFloat("Refraction Index", Group.mRefractionIndex, 0.01f, 0.0f, 10.0f, [this, &Material, GroupIndex](float Value) {
        ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) {
            Target.mRefractionIndex = Value;
        });
    });
    mPropertyEditor.DrawFloat("Opacity", Group.mOpacity, 0.01f, 0.0f, 1.0f, [this, &Material, GroupIndex](float Value) {
        ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) {
            Target.mOpacity = Value;
        });
    });
    mPropertyEditor.DrawFloat("Sharpness", Group.mSharpness, 1.0f, 0.0f, 1000.0f, [this, &Material, GroupIndex](float Value) {
        ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) {
            Target.mSharpness = Value;
        });
    });
    int IlluminationModel{Group.mIlluminationModel};
    if (ImGui::InputInt("Illumination Model", &IlluminationModel)) {
        ModifyGroup(Material, GroupIndex, [IlluminationModel](FMaterialGroup& Target) {
            Target.mIlluminationModel = IlluminationModel;
        });
    }
    mPropertyEditor.DrawBool("Dissolve Halo", Group.mBDissolveHalo, [this, &Material, GroupIndex](bool Value) {
        ModifyGroup(Material, GroupIndex, [Value](FMaterialGroup& Target) {
            Target.mBDissolveHalo = Value;
        });
    });
    ImGui::SeparatorText("Textures");
    for (const FTextureField& Field : TextureFields) {
        DrawTexture(Material, GroupIndex, Field.mLabel, Field.mMember);
    }
}

void FMaterialEditorPanel::DrawTexture(USurfaceOpaque& Material, Uint32 GroupIndex, const char* Label, FMaterialTextureMap FMaterialGroup::* Member) {
    const FAssetHandle CurrentHandle{(Material.GetGroups()[GroupIndex].*Member).mTexture};
    mPropertyEditor.DrawAssetPicker(Label, *UTexture::StaticTypeInfo(), CurrentHandle, [this, &Material, GroupIndex, Member](FAssetHandle NewHandle) {
        ModifyGroup(Material, GroupIndex, [Member, NewHandle](FMaterialGroup& Group) {
            FMaterialTextureMap& Map{Group.*Member};
            Map.mTexture = NewHandle;
            Map.mSourcePath.clear();
        });
    });
}

void FMaterialEditorPanel::ModifyGroup(USurfaceOpaque& Material, Uint32 GroupIndex, const std::function<void(FMaterialGroup&)>& Modifier) {
    if (Material.ModifyGroup(GroupIndex, Modifier)) {
        mPreviewDirty = true;
    }
}
