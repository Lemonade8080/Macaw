#include "ModelResource.hlsli"

struct FSurfaceOpaqueMaterial {
    float4 DiffuseColorAndOpacity;
    float4 AmbientColorAndShininess;
    float4 SpecularColorAndRefractionIndex;
    float4 EmissiveColorAndSharpness;
    float4 TransmissionFilter;
    int IlluminationModel;
    uint DissolveHalo;
    // Paddings
    float2 Padding;
    float4 Reserved1;
    float4 Reserved2;
};

StructuredBuffer<FSurfaceOpaqueMaterial> MaterialBuffer : register(t1);

Texture2D AmbientTexture : register(t3);
Texture2D DiffuseTexture : register(t4);
Texture2D SpecularTexture : register(t5);
Texture2D EmissiveTexture : register(t6);
Texture2D TransmissionTexture : register(t7);
Texture2D ShininessTexture : register(t8);
Texture2D OpacityTexture : register(t9);
Texture2D BumpTexture : register(t10);
Texture2D NormalTexture : register(t11);
Texture2D DisplacementTexture : register(t12);
Texture2D DecalTexture : register(t13);
Texture2D ReflectionTexture : register(t14);

SamplerState LinearWrap : register(s0);

#include "FrameResource.hlsli"

struct VS_INPUT {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 WorldPosition : TEXCOORD1;
    nointerpolation uint MaterialIndex : Jungle1;
    nointerpolation float LODDither : TEXCOORD7;
};

PS_INPUT mainVS(VS_INPUT Input, uint DrawRecordIndex : MODEL_INDEX) {
    PS_INPUT Output;

    FModelContext ModelContext = {GetModelContext(DrawRecordIndex)};

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), ModelContext.mWorld);
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Normal = mul(Input.Normal, (float3x3) ModelContext.mWorld);
    Output.UV = Input.UV;
    Output.WorldPosition = WorldPosition.xyz;
    Output.MaterialIndex = ModelContext.mMaterialIndex;
    Output.LODDither = ModelContext.mLODDither;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET {
    ApplyLODDither(Input.Position.xy, Input.LODDither);
    FSurfaceOpaqueMaterial Material = MaterialBuffer[Input.MaterialIndex];

    float4 BaseColor = DiffuseTexture.Sample(LinearWrap, Input.UV) * Material.DiffuseColorAndOpacity;
    
    return BaseColor;
}
