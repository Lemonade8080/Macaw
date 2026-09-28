struct FModelContext
{
    row_major float4x4 World;
    uint MaterialIndex;
    uint Flags;
};

StructuredBuffer<FModelContext> ModelContexts : register(t0);
Texture2D BaseColorTexture : register(t4);
SamplerState LinearWrap : register(s0);

#include "FrameResource.hlsli"
#include "MeshDraw.hlsli"

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
    nointerpolation uint MaterialIndex : Jungle1;
    nointerpolation float3 ColorCoefficient : Jungle2;
};

PS_INPUT mainVS(VS_INPUT Input, uint InstanceID : SV_InstanceID)
{
    PS_INPUT Output;

    FModelContext ModelContext = ModelContexts[ModelContextStart + InstanceID];
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), ModelContext.World);

    float4x4 view = View;
    view[3][0] = 0.0f;
    view[3][1] = 0.0f;
    view[3][2] = 0.0f;

    float4x4 vp = mul(view, Projection);

    Output.Position = mul(WorldPosition, vp);
    Output.Normal = mul(Input.Normal, (float3x3)ModelContext.World);
    Output.UV = Input.UV;
    Output.MaterialIndex = ModelContext.MaterialIndex;
    Output.ColorCoefficient = float3(1.f, 1.f, 1.f);

    Output.Position.z = Output.Position.w;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 Color = BaseColorTexture.Sample(LinearWrap, Input.UV);
    Color.rgb *= Input.ColorCoefficient;
    return Color;
}
