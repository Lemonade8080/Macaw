Texture2D SpriteTexture : register(t3);
SamplerState LinearWrap : register(s0);
SamplerState LinearClamp : register(s1);

#include "FrameResource.hlsli"

struct VS_INPUT {
    float3 mOrigin : POSITION;
    float2 mSize : SIZE;
    float2 mUvMin : TEXCOORD0;
    float2 mUvMax : TEXCOORD1;
    float4 mColor : COLOR0;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

VS_INPUT mainVS(VS_INPUT Input) {
    return Input;
}

[maxvertexcount(4)] 
void mainGS(point VS_INPUT Input[1], inout TriangleStream<PS_INPUT> Stream)
{
    const VS_INPUT Data = {Input[0]};
    const float3 Origin = {Data.mOrigin};

    float3 CameraRight = normalize(CameraWorld[0].xyz);
    float3 CameraUp = normalize(CameraWorld[1].xyz);

    float HalfW = Data.mSize.x * 0.5;
    float HalfH = Data.mSize.y * 0.5;

    float3 TopLeft = Origin - CameraRight * HalfW + CameraUp * HalfH;
    float3 BottomLeft = Origin - CameraRight * HalfW - CameraUp * HalfH;
    float3 TopRight = Origin + CameraRight * HalfW + CameraUp * HalfH;
    float3 BottomRight = Origin + CameraRight * HalfW - CameraUp * HalfH;

    PS_INPUT Output;
    Output.Color = Data.mColor;

    Output.Position = mul(float4(TopLeft, 1.0f), ViewProjection);
    Output.UV = float2(Data.mUvMin.x, Data.mUvMin.y);
    Stream.Append(Output);

    Output.Position = mul(float4(BottomLeft, 1.0f), ViewProjection);
    Output.UV = float2(Data.mUvMin.x, Data.mUvMax.y);
    Stream.Append(Output);

    Output.Position = mul(float4(TopRight, 1.0f), ViewProjection);
    Output.UV = float2(Data.mUvMax.x, Data.mUvMin.y);
    Stream.Append(Output);

    Output.Position = mul(float4(BottomRight, 1.0f), ViewProjection);
    Output.UV = float2(Data.mUvMax.x, Data.mUvMax.y);
    Stream.Append(Output);

    Stream.RestartStrip();
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 TextColor = SpriteTexture.Sample(LinearWrap, Input.UV);
    float4 ResultColor = TextColor * Input.Color;

    return ResultColor;
}
