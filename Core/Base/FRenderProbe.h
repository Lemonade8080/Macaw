#pragma once
#include "Core/Base/FAssetHandle.h"
#include "Core/Render/FLineRenderData.h"

struct FTextVertex {
    FVector2 mLocalPosition{};
    FVector2 mSize{};
    FVector2 mUvMin{};
    FVector2 mUvMax{};
};

struct FTextProbe {
    FMatrix mWorld{};
    FAssetHandle mFontHandle{};
    FAssetHandle mPipelineHandle{};
    FVector4 mColor{1.0f, 1.0f, 1.0f, 1.0f};
    FVector3 mScreenBoundsExtent{};
    float mScreenUpPadding{};
    TArray<FTextVertex> mVertices{};
};

struct FBillboardProbe {
    FMatrix mWorld{};
    FAssetHandle mTextureHandle{};
    FAssetHandle mPipelineHandle{};
    FVector2 mSize{};
    FVector2 mUvMin{};
    FVector2 mUvMax{};
    FVector4 mColor{};
};

enum class ERenderObjectFlags : Uint32 {
    None = 0,
    Selected = 1u << 0,
    Unlit = 1u << 1
};

enum class ELightType : Uint32 {
    Directional,
    Point,
    Spot
};

Uint32 operator|(ERenderObjectFlags Left, ERenderObjectFlags Right);

struct FActorProbe {
    FMatrix mWorld{};
    FAssetHandle mMeshHandle{};
    FAssetHandle mMaterialHandle{};
    FAssetHandle mPipelineHandle{};
    Uint32 mFlags{0x0000'0000};
};

struct CameraProbe {
    FMatrix mViewProjection{};
    FMatrix mView{};
    FMatrix mProjection{};
};

struct FRenderSettings {
    FVector4 mClearColor{0.2f, 0.2f, 0.7f, 1.0f};
    bool mBRenderSky{true};
};

struct FLightProbe {
    FVector3 mColor{1.0f, 1.0f, 1.0f};
    float mIntensity{1.0f};
    FVector3 mPosition{};
    float mAttenuationRadius{};
    FVector3 mDirection{0.0f, 0.0f, 1.0f};
    float mInnerConeCos{1.0f};
    float mOuterConeCos{1.0f};
    ELightType mType{ELightType::Directional};
    FVector2 mPadding{};
};

static_assert(sizeof(FLightProbe) == 64);

struct FRenderProbe {
    TArray<FActorProbe> mActorProbes{};
    TArray<FActorProbe> mGizmoProbes{};
    TArray<FTextProbe> mTextProbes{};
    TArray<FBillboardProbe> mBillboardProbes{};
    TArray<FLightProbe> mLightProbes{};
    FLineRenderData mSceneGuides{};
    FVector4 mGridFade{};
    bool mBForceUnlit{false};
};
