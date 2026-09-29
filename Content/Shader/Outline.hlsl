struct FModelContext {
    row_major float4x4 mWorld;
    uint mMaterialIndex;
    uint mFlags;
};

StructuredBuffer<FModelContext> ModelContexts : register(t0);

#include "FrameResource.hlsli"

struct FOutlineInput {
    float3 mPosition : POSITION;
    float3 mNormal : NORMAL;
};

struct FOutlineVertex {
    float4 mPosition : SV_POSITION;
};

struct FOutlineGeometryInput {
    float4 mPosition : SV_POSITION;
    float3 mNormal : NORMAL;
};

float4 ExpandOutline(float4 ClipPosition, float3 WorldNormal) {
    const float NormalLengthSquared = {dot(WorldNormal, WorldNormal)};
    if (!(NormalLengthSquared > 0.0f) || ClipPosition.w <= 0.00001f) {
        return ClipPosition;
    }
    WorldNormal *= rsqrt(NormalLengthSquared);
    const float4 ClipNormal = {mul(float4(WorldNormal, 0.0f), ViewProjection)};
    const float2 ProjectedNormal = {(ClipNormal.xy - ClipPosition.xy * (ClipNormal.w / ClipPosition.w)) * Viewport.xy};
    const float2 Direction = {ProjectedNormal / max(length(ProjectedNormal), 0.01f)};
    const float OutlineWidth = {2.0f};
    ClipPosition.xy += Direction * (OutlineWidth * 2.0f * Viewport.zw) * ClipPosition.w;
    return ClipPosition;
}

FOutlineVertex MainVS(FOutlineInput Input, uint ModelIndex : MODEL_INDEX) {
    FOutlineVertex Output = {0.0f, 0.0f, 0.0f, 0.0f};
    const FModelContext ModelContext = {ModelContexts[ModelIndex]};
    const float3 FirstCofactor = {cross(ModelContext.mWorld[1].xyz, ModelContext.mWorld[2].xyz)};
    const float3 SecondCofactor = {cross(ModelContext.mWorld[2].xyz, ModelContext.mWorld[0].xyz)};
    const float3 ThirdCofactor = {cross(ModelContext.mWorld[0].xyz, ModelContext.mWorld[1].xyz)};
    const float Determinant = {dot(ModelContext.mWorld[0].xyz, FirstCofactor)};
    const float3 WorldNormal = {(Input.mNormal.x * FirstCofactor + Input.mNormal.y * SecondCofactor + Input.mNormal.z * ThirdCofactor) * (Determinant < 0.0f ? -1.0f : 1.0f)};
    const float4 WorldPosition = {mul(float4(Input.mPosition, 1.0f), ModelContext.mWorld)};
    Output.mPosition = ExpandOutline(mul(WorldPosition, ViewProjection), WorldNormal);
    return Output;
}

[maxvertexcount(3)]
void MainGS(triangle FOutlineGeometryInput Input[3], inout TriangleStream<FOutlineVertex> Stream) {
    for (uint Index = {0}; Index < 3; ++Index) {
        FOutlineVertex Output = {ExpandOutline(Input[Index].mPosition, Input[Index].mNormal)};
        Stream.Append(Output);
    }
    Stream.RestartStrip();
}

float4 MainPS() : SV_TARGET {
    return float4(1.0f, 1.0f, 0.0f, 1.0f);
}
