struct FModelContext {
    row_major float4x4 World;
    uint MaterialIndex;
    uint Flags;
};

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

StructuredBuffer<FModelContext> ModelContexts : register(t0);
StructuredBuffer<FSurfaceOpaqueMaterial> MaterialBuffer : register(t1);
#include "Lighting.hlsli"

#include "FrameResource.hlsli"
#include "MeshDraw.hlsli"

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
    nointerpolation uint Flags : Jungle3;
};

PS_INPUT mainVS(VS_INPUT Input, uint InstanceID : SV_InstanceID) {
    PS_INPUT Output;

    FModelContext ModelContext = ModelContexts[ModelContextStart + InstanceID];

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), ModelContext.World);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Normal = mul(Input.Normal, (float3x3) ModelContext.World);
    Output.UV = Input.UV;
    Output.WorldPosition = WorldPosition.xyz;
    Output.MaterialIndex = ModelContext.MaterialIndex;
    Output.Flags = ModelContext.Flags;
    Output.ColorCoefficient = float3(1.0f, 1.0f, 1.0f);
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET {
    FSurfaceOpaqueMaterial Material = MaterialBuffer[Input.MaterialIndex];
    float4 BaseColor = Material.DiffuseColorAndOpacity;

    if ((Input.Flags & 2u) != 0) {
        BaseColor.rgb *= Input.ColorCoefficient;
    }
    else {
        BaseColor.rgb *= Input.ColorCoefficient * CalculateDirectLighting(Input.WorldPosition, Input.Normal, LightCount);
    }
    
    return BaseColor;
}
