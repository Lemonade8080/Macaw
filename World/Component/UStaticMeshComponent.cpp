#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UStaticMeshComponent.h"

#include "Core/Base/FRenderProbe.h"
#include "World/AActor.h"
#include "World/UWorld.h"
#include "World/Subsystem/URenderSubsystem.h"
#include "Core/Archive/FArchive.h"
#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UMaterial.h"
#include "Asset/Pipeline/UPipeline.h"

namespace {
    constexpr char BasePipelinePath[]{"/Game/Pipeline/Base"};
    constexpr char TextureBasePipelinePath[]{"/Game/Pipeline/TexturedBase"};
}

FAssetHandle UStaticMeshComponent::GetMaterialHandle() const {
    return mMaterialHandle;
}

FAssetHandle UStaticMeshComponent::GetPipelineHandle() const {
    return mPipelineHandle;
}

void UStaticMeshComponent::SetMeshHandle(FAssetHandle InHandle) {
    const FAssetHandle PreviousHandle{GetMeshHandle()};
    UMeshComponent::SetMeshHandle(InHandle);
    if (PreviousHandle != GetMeshHandle()) {
        NotifyRenderStateChanged();
    }
}

void UStaticMeshComponent::SetMaterialHandle(FAssetHandle InHandle) {
    const FAssetHandle PreviousMaterialHandle{mMaterialHandle};
    const FAssetHandle PreviousPipelineHandle{mPipelineHandle};
    mMaterialHandle = InHandle;
    AActor* Owner{GetOwner()};
    UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mMaterialAssetPath = Registry != nullptr && Registry->GetAssetPath(mMaterialHandle) != nullptr ? *Registry->GetAssetPath(mMaterialHandle) : FAssetPath{};
    mMaterialAssetGuid = Registry != nullptr && Registry->GetAssetGuid(mMaterialHandle) != nullptr ? *Registry->GetAssetGuid(mMaterialHandle) : FGuid{};
    EnsureDefaultRenderAssets();
    if (PreviousMaterialHandle != mMaterialHandle || PreviousPipelineHandle != mPipelineHandle) {
        NotifyRenderStateChanged();
    }
}

void UStaticMeshComponent::SetPipelineHandle(FAssetHandle InHandle) {
    const FAssetHandle PreviousMaterialHandle{mMaterialHandle};
    const FAssetHandle PreviousPipelineHandle{mPipelineHandle};
    mPipelineHandle = InHandle;
    AActor* Owner{GetOwner()};
    UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mPipelineAssetPath = Registry != nullptr && Registry->GetAssetPath(mPipelineHandle) != nullptr ? *Registry->GetAssetPath(mPipelineHandle) : FAssetPath{};
    mPipelineAssetGuid = Registry != nullptr && Registry->GetAssetGuid(mPipelineHandle) != nullptr ? *Registry->GetAssetGuid(mPipelineHandle) : FGuid{};
    EnsureDefaultRenderAssets();
    if (PreviousMaterialHandle != mMaterialHandle || PreviousPipelineHandle != mPipelineHandle) {
        NotifyRenderStateChanged();
    }
}

void UStaticMeshComponent::NotifyRenderStateChanged() {
    AActor* Owner{GetOwner()};
    UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
    if (World != nullptr) {
        World->GetRenderSubsystem().UpdateComponentRenderState(this);
    }
}

void UStaticMeshComponent::OnRegister() {
    UMeshComponent::OnRegister();

    EnsureDefaultRenderAssets();

    AActor* Owner{GetOwner()};

    if (Owner != nullptr && Owner->GetWorld() != nullptr) {
        Owner->GetWorld()->GetRenderSubsystem().RegisterComponent(this);
    }
}

void UStaticMeshComponent::EnsureDefaultRenderAssets() {
    AActor* Owner{GetOwner()};
    UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    if (Registry == nullptr) {
        return;
    }

    if (Registry->ResolveAsset<UMaterial>(mMaterialHandle) == nullptr) {
        mMaterialHandle = Registry->EnsureDefaultStaticMeshMaterial();
    }

    if (Registry->ResolveAsset<UPipeline>(mPipelineHandle) == nullptr) {
        mPipelineHandle = Registry->EnsureDefaultStaticMeshPipeline();
    }
}

void UStaticMeshComponent::OnUnregister() {
    AActor* Owner{GetOwner()};

    if (Owner != nullptr && Owner->GetWorld() != nullptr) {
        Owner->GetWorld()->GetRenderSubsystem().UnregisterComponent(this);
    }

    UMeshComponent::OnUnregister();
}

void UStaticMeshComponent::MakeRender(FActorProbe& OutProbe) const {
    if (!IsActive() || !IsVisible()) {
        return;
    }

    OutProbe = FActorProbe{ GetComponentToWorld(), GetMeshHandle(), mMaterialHandle, mPipelineHandle, 0x0000'0000};
    OutProbe.mLocalBounds = GetBoundingSphere();
}

void UStaticMeshComponent::Serialize(FArchive& Archive) {
    UMeshComponent::Serialize(Archive);

    const IAssetRegistry* Registry{Archive.GetAssetRegistry()};
    if (Archive.IsSaving() && Registry != nullptr) {
        if (const FAssetPath* AssetPath{Registry->GetAssetPath(mMaterialHandle)}) {
            mMaterialAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{Registry->GetAssetGuid(mMaterialHandle)}) {
            mMaterialAssetGuid = *AssetGuid;
        }
        if (const FAssetPath* AssetPath{Registry->GetAssetPath(mPipelineHandle)}) {
            mPipelineAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{Registry->GetAssetGuid(mPipelineHandle)}) {
            mPipelineAssetGuid = *AssetGuid;
        }
    }

    Archive.Serialize("MaterialAssetGuid", mMaterialAssetGuid);
    Archive.Serialize("MaterialAssetPath", mMaterialAssetPath.mPath);
    Archive.Serialize("PipelineAssetGuid", mPipelineAssetGuid);
    Archive.Serialize("PipelineAssetPath", mPipelineAssetPath.mPath);
    if (Archive.IsLoading()) {
        mMaterialHandle = Registry != nullptr ? Registry->FindAsset(mMaterialAssetGuid) : FAssetHandle{};
        if (!mMaterialHandle && Registry != nullptr) {
            mMaterialHandle = Registry->FindAsset(mMaterialAssetPath);
        }
        mPipelineHandle = Registry != nullptr ? Registry->FindAsset(mPipelineAssetGuid) : FAssetHandle{};
        if (!mPipelineHandle && Registry != nullptr) {
            mPipelineHandle = Registry->FindAsset(mPipelineAssetPath);
        }
        NotifyRenderStateChanged();
    }
}

void UStaticMeshComponent::DrawPanels(IPropertyEditorContext* Context) {
    UMeshComponent::DrawPanels(Context);

    Context->DrawAssetPicker("Material", *UMaterial::StaticTypeInfo(), GetMaterialHandle(), [this](FAssetHandle Handle) {
        SetMaterialHandle(Handle);

        AActor* Owner{GetOwner()};
        UWorld* World{Owner != nullptr ? Owner->GetWorld() : nullptr};
        const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
        if (Registry == nullptr) {
            return;
        }

        const UMaterial* Material{Registry->ResolveAsset<UMaterial>(GetMaterialHandle())};
        bool HasTexture{};
        if (Material != nullptr) {
            for (Uint32 GroupIndex{}; GroupIndex < Material->GetGPUDataCount() && !HasTexture; ++GroupIndex) {
                const FMaterialChunkSignature Signature{Material->BuildChunkSignature(GroupIndex)};
                for (Uint8 TextureFieldIndex{}; TextureFieldIndex < Signature.mTextureFieldCount; ++TextureFieldIndex) {
                    if (Signature.GetTextureHandle(TextureFieldIndex)) {
                        HasTexture = true;
                        break;
                    }
                }
            }
        }

        const FAssetHandle DesiredPipelineHandle{Registry->FindAsset(FAssetPath{HasTexture ? TextureBasePipelinePath : BasePipelinePath})};
        if (DesiredPipelineHandle != GetPipelineHandle() && Registry->ResolveAsset<UPipeline>(DesiredPipelineHandle) != nullptr) {
            SetPipelineHandle(DesiredPipelineHandle);
        }
    });
    Context->DrawAssetPicker("Pipeline", *UPipeline::StaticTypeInfo(), GetPipelineHandle(), [this](FAssetHandle Handle) {
        SetPipelineHandle(Handle);
    });
}
