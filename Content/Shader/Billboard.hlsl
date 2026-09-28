struct FBillboardData
{
    row_major float4x4 World;
    float2 Size;
    float2 UVMin;
    float2 UVMax;
    float2 Pad;
    float4 Color;
};

StructuredBuffer<FBillboardData> Billboards : register(t0);

Texture2D SpriteTexture : register(t3);
SamplerState LinearWrap : register(s0);
SamplerState LinearClamp : register(s1);

#include "FrameResource.hlsli"

struct VS_OUTPUT
{
    uint InstanceID : INSTANCE_ID;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

VS_OUTPUT mainVS(uint InstanceID : SV_InstanceID)
{
    VS_OUTPUT Output;    
    Output.InstanceID = InstanceID;    
    return Output;
}

[maxvertexcount(4)] 
void mainGS(point VS_OUTPUT Input[1], inout TriangleStream<PS_INPUT> Stream)
{
    uint BillboardIndex = Input[0].InstanceID;
    FBillboardData Data = Billboards[BillboardIndex];

    float3 Origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), Data.World).xyz;

    float3 CameraRight = normalize(CameraWorld[0].xyz);
    float3 CameraUp = normalize(CameraWorld[1].xyz);

    float HalfW = Data.Size.x * 0.5;
    float HalfH = Data.Size.y * 0.5;

    float3 TopLeft = Origin - CameraRight * HalfW + CameraUp * HalfH;
    float3 BottomLeft = Origin - CameraRight * HalfW - CameraUp * HalfH;
    float3 TopRight = Origin + CameraRight * HalfW + CameraUp * HalfH;
    float3 BottomRight = Origin + CameraRight * HalfW - CameraUp * HalfH;

    PS_INPUT Output;
    Output.Color = Data.Color;

    Output.Position = mul(float4(TopLeft, 1.0f), ViewProjection);
    Output.UV = float2(Data.UVMin.x, Data.UVMin.y);
    Stream.Append(Output);

    Output.Position = mul(float4(BottomLeft, 1.0f), ViewProjection);
    Output.UV = float2(Data.UVMin.x, Data.UVMax.y);
    Stream.Append(Output);

    Output.Position = mul(float4(TopRight, 1.0f), ViewProjection);
    Output.UV = float2(Data.UVMax.x, Data.UVMin.y);
    Stream.Append(Output);

    Output.Position = mul(float4(BottomRight, 1.0f), ViewProjection);
    Output.UV = float2(Data.UVMax.x, Data.UVMax.y);
    Stream.Append(Output);

    Stream.RestartStrip();
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 TextColor = SpriteTexture.Sample(LinearWrap, Input.UV);
    float4 ResultColor = TextColor * Input.Color;

    return ResultColor;
}
