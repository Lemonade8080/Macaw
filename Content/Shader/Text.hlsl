Texture2D FontAtlas : register(t3);
SamplerState PointClamp : register(s2);

#include "FrameResource.hlsli"
cbuffer TextConstants : register(b2) {
    row_major float4x4 World;
    float4 TextColor;
    float3 ScreenBoundsExtent;
    float ScreenUpPadding;
};

struct VS_INPUT
{
    float2 LocalPosition : POSITION;
    float2 Size : SIZE;
    float2 UVMin : TEXCOORD0;
    float2 UVMax : TEXCOORD1;
};

struct VS_OUTPUT
{
    float2 LocalPosition : POSITION;
    float2 Size : SIZE;
    float2 UVMin : TEXCOORD0;
    float2 UVMax : TEXCOORD1;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT mainVS(VS_INPUT Input)
{
    VS_OUTPUT Output;

    Output.LocalPosition = Input.LocalPosition;
    Output.Size = Input.Size;
    Output.UVMax = Input.UVMax;
    Output.UVMin = Input.UVMin;

    return Output;
}

[maxvertexcount(4)] // Geometry Shader가 입력 하나당 최대 몇개의 정점을 만들지 GPU에게 알림
void mainGS(point VS_OUTPUT Input[1], inout TriangleStream<PS_INPUT> Stream) // Geometry Shader가 점 하나를 입력으로 받는다. , Geometry Shader가 만든 정점을 삼각형 스트림 형태로 다음 단계에 전달하는 출력 통로
{
    VS_OUTPUT Glyph = Input[0];

    float3 Origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), World).xyz;
    float3 CameraRight = normalize(CameraWorld[0].xyz);
    float3 CameraUp = normalize(CameraWorld[1].xyz);
    float BoundsScreenHalfHeight = dot(abs(CameraUp), ScreenBoundsExtent);
    Origin += CameraUp * (BoundsScreenHalfHeight + ScreenUpPadding);

    float Left = Glyph.LocalPosition.x;
    float Right = Left + Glyph.Size.x;
    float Top = Glyph.LocalPosition.y;
    float Bottom = Top - Glyph.Size.y;

    float3 TopLeft = Origin + CameraRight * Left + CameraUp * Top;
    float3 BottomLeft = Origin + CameraRight * Left + CameraUp * Bottom;
    float3 TopRight = Origin + CameraRight * Right + CameraUp * Top;
    float3 BottomRight = Origin + CameraRight * Right + CameraUp * Bottom;

    PS_INPUT Output;

    Output.Position = mul(float4(TopLeft, 1.0f), ViewProjection);
    Output.UV = float2(Glyph.UVMin.x, Glyph.UVMin.y);
    Stream.Append(Output); // 사각형의 왼쪽 위 꼭짓점 하나를 GPU 출력 스트림에 추가한다.

    Output.Position = mul(float4(BottomLeft, 1.0f), ViewProjection);
    Output.UV = float2(Glyph.UVMin.x, Glyph.UVMax.y);
    Stream.Append(Output);

    Output.Position = mul(float4(TopRight, 1.0f), ViewProjection);
    Output.UV = float2(Glyph.UVMax.x, Glyph.UVMin.y);
    Stream.Append(Output);

    Output.Position = mul(float4(BottomRight, 1.0f), ViewProjection);
    Output.UV = float2(Glyph.UVMax.x, Glyph.UVMax.y);
    Stream.Append(Output);

    Stream.RestartStrip(); // 현재 문자의 사각형이 끝남
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 AtlasColor = FontAtlas.SampleLevel(PointClamp, Input.UV, 0.0f); // Atlas 텍스처의 Input.UV 위치 색상을 읽어라.
    float Coverage = AtlasColor.r;

    return float4(TextColor.rgb, TextColor.a * Coverage);
}
