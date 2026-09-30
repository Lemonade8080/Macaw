#include "ModelResource.hlsli"

struct FSurfaceOpaqueMaterial {
    float4 DiffuseColorAndOpacity;
    float4 AmbientColorAndShininess;
    float4 SpecularColorAndRefractionIndex;
    float4 EmissiveColorAndSharpness;
    float4 TransmissionFilter;
    int IlluminationModel;
    uint DissolveHalo;
    float2 Padding;
    float4 Reserved1;
    float4 Reserved2;
};

StructuredBuffer<FSurfaceOpaqueMaterial> MaterialBuffer : register(t1);

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
    nointerpolation float3 ColorCoefficient : Jungle2;
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
    Output.ColorCoefficient = float3(1.0f, 1.0f, 1.0f);

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET {
    FSurfaceOpaqueMaterial Material = MaterialBuffer[Input.MaterialIndex];
    float4 BaseColor = Material.DiffuseColorAndOpacity;

    BaseColor.rgb *= Input.ColorCoefficient;

    return BaseColor;
}
