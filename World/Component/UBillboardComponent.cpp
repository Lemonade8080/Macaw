#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UBillboardComponent.h"

#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UTexture.h"

#include "Asset/Pipeline/UPipeline.h"

#include "World/AActor.h"
#include "World/UWorld.h"
#include "World/Subsystem/UBillboardSubsystem.h"

bool UBillboardComponent::CanRenderBillBoard() const {
    return IsActive() && IsVisible();
}

void UBillboardComponent::Serialize(FArchive& Archive) {
    UPrimitiveComponent::Serialize(Archive);

    const IAssetRegistry* Registry{Archive.GetAssetRegistry()};
    if (Archive.IsSaving() && Registry != nullptr) {
        if (const FAssetPath* AssetPath{Registry->GetAssetPath(mTextureHandle)}) {
            mTextureAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{Registry->GetAssetGuid(mTextureHandle)}) {
            mTextureAssetGuid = *AssetGuid;
        }
        if (const FAssetPath* AssetPath{Registry->GetAssetPath(mPipelineHandle)}) {
            mPipelineAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{Registry->GetAssetGuid(mPipelineHandle)}) {
            mPipelineAssetGuid = *AssetGuid;
        }
    }
    Archive.Serialize("TextureAssetGuid", mTextureAssetGuid);
    Archive.Serialize("TextureAssetPath", mTextureAssetPath.mPath);
    Archive.Serialize("PipelineAssetGuid", mPipelineAssetGuid);
    Archive.Serialize("PipelineAssetPath", mPipelineAssetPath.mPath);
    if (Archive.IsLoading()) {
        mTextureHandle = Registry != nullptr ? Registry->FindAsset(mTextureAssetGuid) : FAssetHandle{};
        if (!mTextureHandle && Registry != nullptr) {
            mTextureHandle = Registry->FindAsset(mTextureAssetPath);
        }
        mPipelineHandle = Registry != nullptr ? Registry->FindAsset(mPipelineAssetGuid) : FAssetHandle{};
        if (!mPipelineHandle && Registry != nullptr) {
            mPipelineHandle = Registry->FindAsset(mPipelineAssetPath);
        }
    }

    Archive.Serialize("Size", mSize);
    Archive.Serialize("UVMin", mUvMin);
    Archive.Serialize("UVMax", mUvMax);
    Archive.Serialize("Color", mColor);
}

bool UBillboardComponent::TryGetBillBoardWorld(FMatrix& OutWorld) const {
    if (!CanRenderBillBoard()) {
        return false;
    }

    OutWorld = GetComponentToWorld();
    return true;
}

void UBillboardComponent::SetTextureHandle(FAssetHandle InTextureHandle) {
    mTextureHandle = InTextureHandle;
    UWorld* World{GetBelongingWorld()};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mTextureAssetPath = Registry != nullptr && Registry->GetAssetPath(mTextureHandle) != nullptr ? *Registry->GetAssetPath(mTextureHandle) : FAssetPath{};
    mTextureAssetGuid = Registry != nullptr && Registry->GetAssetGuid(mTextureHandle) != nullptr ? *Registry->GetAssetGuid(mTextureHandle) : FGuid{};
}

void UBillboardComponent::SetPipelineHandle(FAssetHandle InPipelineHandle) {
    mPipelineHandle = InPipelineHandle;
    UWorld* World{GetBelongingWorld()};
    const IAssetRegistry* Registry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mPipelineAssetPath = Registry != nullptr && Registry->GetAssetPath(mPipelineHandle) != nullptr ? *Registry->GetAssetPath(mPipelineHandle) : FAssetPath{};
    mPipelineAssetGuid = Registry != nullptr && Registry->GetAssetGuid(mPipelineHandle) != nullptr ? *Registry->GetAssetGuid(mPipelineHandle) : FGuid{};
}

void UBillboardComponent::SetSize(const FVector2& InSize) {
    mSize = InSize;
}

void UBillboardComponent::SetUV(const FVector2& InUVMin, const FVector2& InUVMax) {
    mUvMin = InUVMin;
    mUvMax = InUVMax;
}

void UBillboardComponent::SetColor(const FVector4& InColor) {
    mColor = InColor;
}

FAssetHandle UBillboardComponent::GetTextureHandle() const {
    return mTextureHandle;
}

FAssetHandle UBillboardComponent::GetPipelineHandle() const {
    return mPipelineHandle;
}

const FVector2& UBillboardComponent::GetSize() const {
    return mSize;
}

const FVector2& UBillboardComponent::GetUVmin() const {
    return mUvMin;
}

const FVector2& UBillboardComponent::GetUVMax() const {
    return mUvMax;
}

const FVector4& UBillboardComponent::GetColor() const {
    return mColor;
}

bool UBillboardComponent::MakeBillboardRender(FBillboardProbe& OutProbe) const {
    if (!mTextureHandle || !mPipelineHandle) {
        return false;
    }

    if (!TryGetBillBoardWorld(OutProbe.mWorld)) {
        return false;
    }

    OutProbe.mTextureHandle = mTextureHandle;
    OutProbe.mPipelineHandle = mPipelineHandle;
    OutProbe.mSize = mSize;
    OutProbe.mUvMin = mUvMin;
    OutProbe.mUvMax = mUvMax;
    OutProbe.mColor = mColor;

    return true;
}

bool UBillboardComponent::GetWorldCorners(const FMatrix& CameraWorld, std::array<FVector3, 4>& OutCorners) const {
    FBillboardProbe Probe{};
    if (!MakeBillboardRender(Probe) || Probe.mSize.mX <= 0.0f || Probe.mSize.mY <= 0.0f) {
        return false;
    }

    FVector3 Right{CameraWorld.m_[0][0], CameraWorld.m_[0][1], CameraWorld.m_[0][2]};
    FVector3 Up{CameraWorld.m_[1][0], CameraWorld.m_[1][1], CameraWorld.m_[1][2]};
    if (Right.LengthSquared() <= 0.0f || Up.LengthSquared() <= 0.0f) {
        return false;
    }
    Right.Normalize();
    Up.Normalize();

    const FVector3 Origin{Probe.mWorld.Translation()};
    const FVector3 Horizontal{Right * (Probe.mSize.mX * 0.5f)};
    const FVector3 Vertical{Up * (Probe.mSize.mY * 0.5f)};
    OutCorners = {Origin - Horizontal + Vertical, Origin - Horizontal - Vertical, Origin + Horizontal + Vertical, Origin + Horizontal - Vertical};
    return true;
}

void UBillboardComponent::OnRegister() {
    UPrimitiveComponent::OnRegister();

    UWorld* World{GetBelongingWorld()};
    if (World != nullptr) {
        World->GetBillboardSubsystem().RegisterComponent(this);
    }
}

void UBillboardComponent::OnUnregister() {
    UWorld* World{GetBelongingWorld()};
    if (World != nullptr) {
        World->GetBillboardSubsystem().UnregisterComponent(this);
    }

    UPrimitiveComponent::OnUnregister();
}

void UBillboardComponent::DrawPanels(IPropertyEditorContext* Context) {
    UPrimitiveComponent::DrawPanels(Context);
    Context->DrawColor("Color", GetColor(), [this](const FVector4& NewColor) {
        SetColor(NewColor);
    });

    Context->DrawAssetPicker("Texture", *UTexture::StaticTypeInfo(), GetTextureHandle(), [this](FAssetHandle NewHandle) {
        SetTextureHandle(NewHandle);
    });
    Context->DrawAssetPicker("Pipeline", *UPipeline::StaticTypeInfo(), GetPipelineHandle(), [this](FAssetHandle NewHandle) {
        SetPipelineHandle(NewHandle);
    });
}
