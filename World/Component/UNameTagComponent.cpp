#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UNameTagComponent.h"
#include "Asset/UFont.h"

#include "Asset/Pipeline/UPipeline.h"

#include "World/AActor.h"
#include "World/Component/UMeshComponent.h"
#include "World/UWorld.h"
#include "Core/Base/UObjectSystem.h"

#include <algorithm>

void UNameTagComponent::SetTargetActor(AActor* InTargetActor) {
    if (InTargetActor == nullptr || InTargetActor == GetOwner()) {
        mTargetActor.Reset();
        mExplicitTargetGuid = {};
    } else {
        mTargetActor.Set(InTargetActor);
        mExplicitTargetGuid = InTargetActor->GetGuid();
    }
    RefreshGuidText();
}

void UNameTagComponent::OnRegister() {
    UBillboardTextComponent::OnRegister();
    RefreshGuidText();
}

AActor* UNameTagComponent::GetTargetActor() const {
    if (!mExplicitTargetGuid.IsValid()) {
        return GetOwner();
    }
    return mTargetActor.Get();
}

void UNameTagComponent::SetTargetLocalOffset(const FVector3& InOffset) {
    mTargetLocalOffset = InOffset;
}

const FVector3& UNameTagComponent::GetTargetLocalOffset() const {
    return mTargetLocalOffset;
}

const FVector3& UNameTagComponent::GetObjectOffset() const {
    return mTargetLocalOffset;
}

FGuid UNameTagComponent::GetObjectGuid() const {
    if (mExplicitTargetGuid.IsValid()) {
        return mExplicitTargetGuid;
    }

    const AActor* Owner{GetOwner()};

    return Owner != nullptr ? Owner->GetGuid() : FGuid{};
}

bool UNameTagComponent::MakeTextRender(FTextProbe& OutProbe) const {
    AActor* Target{GetTargetActor()};

    if (Target == nullptr || Target->GetRootComponent() == nullptr) {
        return false;
    }

    if (!UBillboardTextComponent::MakeTextRender(OutProbe)) {
        return false;
    }

    const FMatrix TargetWorld{Target->GetActorTransform().ToMatrixWithScale()};
    const FVector3 TargetOrigin{TargetWorld.Translation()};
    FVector3 Minimum{TargetOrigin};
    FVector3 Maximum{TargetOrigin};
    bool HasMeshBounds{false};

    for (const std::unique_ptr<UActorComponent>& Component : Target->GetComponents()) {
        if (!Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
            continue;
        }

        const UMeshComponent* MeshComponent{static_cast<const UMeshComponent*>(Component.get())};
        if (!MeshComponent->GetMeshHandle()) {
            continue;
        }

        DirectX::BoundingOrientedBox WorldBox{};
        MeshComponent->GetPickingBox().Transform(WorldBox, MeshComponent->GetComponentToWorld().ToSimpleMath());
        DirectX::XMFLOAT3 Corners[DirectX::BoundingOrientedBox::CORNER_COUNT]{};
        WorldBox.GetCorners(Corners);

        for (const DirectX::XMFLOAT3& Corner : Corners) {
            const FVector3 Position{Corner};
            if (!HasMeshBounds) {
                Minimum = Position;
                Maximum = Position;
                HasMeshBounds = true;
            } else {
                Minimum.mX = std::min(Minimum.mX, Position.mX);
                Minimum.mY = std::min(Minimum.mY, Position.mY);
                Minimum.mZ = std::min(Minimum.mZ, Position.mZ);
                Maximum.mX = std::max(Maximum.mX, Position.mX);
                Maximum.mY = std::max(Maximum.mY, Position.mY);
                Maximum.mZ = std::max(Maximum.mZ, Position.mZ);
            }
        }
    }

    const FVector3 Center{HasMeshBounds ? (Minimum + Maximum) * 0.5f : TargetOrigin};

    // TargetLocalOffset이 Target의 로컬 공간 Offset이므로 Target의 회전과 scale까지 적용한다.
    const FVector3 Offset{TargetWorld.TransformPosition(mTargetLocalOffset) - TargetOrigin};

    // NameTag 컴포넌트 자신의 scale 등은 유지하고, 렌더링 원점만 Target 위치로 교체한다.
    // 현재 Text Shader는 World에서 translation만 사용하므로 실질적으로 AnchorWorld가 Billboard 원점이 된다.
    OutProbe.mWorld.Translation(Center + Offset);
    OutProbe.mScreenBoundsExtent = HasMeshBounds ? (Maximum - Minimum) * 0.5f : FVector3{};
    OutProbe.mScreenUpPadding = GetCharacterHeight() * 0.5f + 0.2f;
    return true;
}

void UNameTagComponent::Serialize(FArchive& Archive) {
    UBillboardTextComponent::Serialize(Archive);
    Archive.Serialize("TargetActorGuid", mExplicitTargetGuid);
    Archive.Serialize("TargetLocalOffset", mTargetLocalOffset);
    if (Archive.IsLoading()) {
        mTargetActor.Reset();
    }
}

bool UNameTagComponent::ResolveLoadedReferences() {
    if (!UBillboardTextComponent::ResolveLoadedReferences()) {
        return false;
    }
    if (!mExplicitTargetGuid.IsValid()) {
        return GetOwner() != nullptr;
    }
    const FObjectHandle TargetHandle{UObjectSystem::FindHandleByGuid(mExplicitTargetGuid)};
    UObject* Object{UObjectSystem::Resolve(TargetHandle)};
    if (Object == nullptr || !Object->GetTypeInfo()->IsA(AActor::StaticTypeInfo())) {
        return false;
    }
    mTargetActor.Set(static_cast<AActor*>(Object));
    return true;
}

void UNameTagComponent::RefreshGuidText() {
    const FGuid TargetGuid{GetObjectGuid()};

    if (TargetGuid.IsValid()) {
        SetText(TargetGuid.ToString());
    } else {
        SetText("");
    }
}

void UNameTagComponent::DrawPanels(IPropertyEditorContext* Context) {
    if (!Context->BeginCategory("Name Tag")) {
        return;
    }

    Context->DrawColor("Color", GetColor(), [this](const FVector4& NewColor) {
        SetColor(NewColor);
    });
    Context->DrawFloat("Character Height", GetCharacterHeight(), 0.01f, 0.001f, 1000.0f, [this](float NewHeight) {
        SetCharacterHeight(NewHeight);
    });
    Context->DrawFloat("Letter Spacing", GetLetterSpacing(), 0.01f, -100.0f, 100.0f, [this](float NewSpacing) {
        SetLetterSpacing(NewSpacing);
    });
    Context->DrawFloat("Line Spacing", GetLineSpacing(), 0.01f, -100.0f, 100.0f, [this](float NewSpacing) {
        SetLineSpacing(NewSpacing);
    });

    Context->DrawAssetPicker("Font", *UFont::StaticTypeInfo(), GetFontHandle(), [this](FAssetHandle NewHandle) {
        SetFontHandle(NewHandle);
    });
    Context->DrawAssetPicker("Pipeline", *UPipeline::StaticTypeInfo(), GetPipelineHandle(), [this](FAssetHandle NewHandle) {
        SetPipelineHandle(NewHandle);
    });
}
