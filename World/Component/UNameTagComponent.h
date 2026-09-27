#pragma once

#include "UBillboardTextComponent.h"

#include "Core/Base/FGuid.h"
#include "Core/Base/TObjectRef.h"
#include "World/AActor.h"

class UNameTagComponent final : public UBillboardTextComponent {
public:
    UNameTagComponent() = default;
    ~UNameTagComponent() override = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UNameTagComponent, UBillboardTextComponent);

    //nullptr을 지정하면 Owner Actor를 Target으로 사용한다.
    void SetTargetActor(AActor* InTargetActor);

    AActor* GetTargetActor() const;

    void SetTargetLocalOffset(const FVector3& InOffset);
    const FVector3& GetTargetLocalOffset() const;

    // 요구사항에 맞춘 접근 함수
    FGuid GetObjectGuid() const;

    const FVector3& GetObjectOffset() const;
    bool MakeTextRender(FTextProbe& OutProbe) const override;
    bool ResolveLoadedReferences() override;
    void RefreshGuidText();
    void OnRegister() override;
    void DrawPanels(IPropertyEditorContext* Context) override;

private:
    void Serialize(FArchive& Archive) override;

private:
    TObjectRef<AActor> mTargetActor{};

    // invalid GUID이면 Owner Actor를 사용한다.
    // TObjectRef가 무효화돼도 원래 Target GUID는 보존된다.
    FGuid mExplicitTargetGuid{};

    // Target Actor의 로컬 좌표 기준 Offset.
    FVector3 mTargetLocalOffset{};
};
