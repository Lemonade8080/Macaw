#include "ModelResource.hlsli"

struct FMaterial
{
    float4 BaseColor;

    float4 Parameters0;
    float4 Parameters1;
    float4 Parameters2;
    float4 Parameters3;
    float4 Parameters4;
    float4 Parameters5;
    float4 Parameters6;
};

StructuredBuffer<FMaterial> MaterialBuffer : register(t1);
#include "Lighting.hlsli"

#include "FrameResource.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 WorldPosition : TEXCOORD1;
    nointerpolation uint MaterialIndex : Jungle1;
    nointerpolation float LODDither : TEXCOORD7;
    nointerpolation float3 ColorCoefficient : Jungle2;
};

PS_INPUT mainVS(VS_INPUT Input, uint DrawRecordIndex : MODEL_INDEX)
{
    PS_INPUT Output;

    FModelContext ModelContext = {GetModelContext(DrawRecordIndex)};

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), ModelContext.mWorld);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Normal = mul(Input.Normal, (float3x3) ModelContext.mWorld);
    Output.UV = Input.UV;
    Output.WorldPosition = WorldPosition.xyz;
    Output.MaterialIndex = ModelContext.mMaterialIndex;
    Output.LODDither = ModelContext.mLODDither;

    if ((ModelContext.mFlags & 1) != 0)
    {
        Output.ColorCoefficient = float3(0.5f, 0.5f, 0.5f);
    }
    else
    {
        Output.ColorCoefficient = float3(1.0f, 1.0f, 1.0f);
    }

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    ApplyLODDither(Input.Position.xy, Input.LODDither);
    float4 MaterialColor = MaterialBuffer[Input.MaterialIndex].BaseColor;
    float Stripe = step(0.5f, frac((Input.UV.x + Input.UV.y) * 6.0f));
    float3 AlternateColor = lerp(MaterialColor.bgr, float3(0.1f, 0.85f, 1.0f), 0.7f);
    float Brightness = lerp(0.4f, 1.0f, Stripe);

    float4 FinalColor = float4(saturate(AlternateColor * Brightness), MaterialColor.a);

    FinalColor.rgb *= Input.ColorCoefficient;
    FinalColor.rgb *= CalculateDirectLighting(Input.WorldPosition, Input.Normal, LightCount);

    return FinalColor;
}
