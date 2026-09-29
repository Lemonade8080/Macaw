#pragma once

#include "USceneComponent.h"
#include "Core/Archive/FArchive.h"

class UCameraComponent : public USceneComponent {
public:
    inline static const FMatrix CameraBasis{
         0.0f, 1.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 1.0f, 0.0f,
         1.0f, 0.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 0.0f, 1.0f};

public:
    UCameraComponent();
    ~UCameraComponent() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(UCameraComponent, USceneComponent)

    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetViewProjectionMatrix() const;

    float GetFOV() const;
    float GetAspectRatio() const;
    float GetNearPlane() const;
    float GetFarPlane() const;

    void SetFOV(float InFOV);
    void SetAspectRatio(float InAspectRatio);
    void SetNearPlane(float InNearPlane);
    void SetFarPlane(float InFarPlane);
    void DrawPanels(IPropertyEditorContext* Context) override;

    void OnRegister() override;
    void OnUnregister() override;

    void SetMoveSensitivity(float InMoveSensitivity);
    float GetMoveSensitivity() const;

    void SetRotationSensitivity(float InRotationSensitivity);
    float GetRotationSensitivity() const;

protected:
    void Serialize(FArchive& Archive) override;

private:
    float mFov{1.0472f}; // 약 60도, 라디안
    float mAspectRatio{16.0f / 9.0f};
    float mNearPlane{0.1f};
    float mFarPlane{1000.0f};
    float mMoveSensitivity{5.0f};
    float mRotationSensitivity{0.1f};
};
