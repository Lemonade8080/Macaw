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
    // Paddings
    float2 Padding;
    float4 Reserved1;
    float4 Reserved2;
};

StructuredBuffer<FModelContext> ModelContexts : register(t0);
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
#include "MeshDraw.hlsli"

struct VS_INPUT {
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    nointerpolation uint MaterialIndex : Jungle1;
};

PS_INPUT mainVS(VS_INPUT Input, uint InstanceID : SV_InstanceID) {
    PS_INPUT Output;
    FModelContext ModelContext = ModelContexts[ModelContextStart + InstanceID];
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), ModelContext.World);
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.UV = Input.UV;
    Output.MaterialIndex = ModelContext.MaterialIndex;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET {
    FSurfaceOpaqueMaterial Material = MaterialBuffer[Input.MaterialIndex];
    float4 TextureColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    uint Width;
    uint Height;
    DiffuseTexture.GetDimensions(Width, Height);

    if (Width > 0u && Height > 0u) {
        TextureColor = DiffuseTexture.Sample(LinearWrap, Input.UV);
    }

    return TextureColor * Material.DiffuseColorAndOpacity;
}
