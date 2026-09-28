#include "FrameResource.hlsli"

struct VS_INPUT
{
    float2 Corner : CORNER;
    float4 StartAndWidth : START;
    float4 EndAndPadding : END;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    noperspective float EdgeDistance : TEXCOORD0;
    nointerpolation float HalfWidth : TEXCOORD1;
    float3 WorldPosition : TEXCOORD2;
    nointerpolation float GridSpacing : TEXCOORD3;
    nointerpolation float2 ScreenNormal : TEXCOORD4;
    nointerpolation float2 GridAxis : TEXCOORD5;
};

bool ClipLineToNearPlane(inout float4 StartClip, inout float4 EndClip, inout float3 StartWorld, inout float3 EndWorld)
{
    const float NearClipEpsilon = 0.00001f;

    float StartDistance = StartClip.z - NearClipEpsilon;
    float EndDistance = EndClip.z - NearClipEpsilon;

    bool StartBehindNearPlane = StartDistance < 0.0f;
    bool EndBehindNearPlane = EndDistance < 0.0f;

    if (StartBehindNearPlane && EndBehindNearPlane)
    {
        return false;
    }

    if (StartBehindNearPlane)
    {
        float Interpolation = saturate(-StartDistance / (EndDistance - StartDistance));
        StartClip = lerp(StartClip, EndClip, Interpolation);
        StartWorld = lerp(StartWorld, EndWorld, Interpolation);
    }
    else if (EndBehindNearPlane)
    {
        float Interpolation = saturate(-EndDistance / (StartDistance - EndDistance));
        EndClip = lerp(EndClip, StartClip, Interpolation);
        EndWorld = lerp(EndWorld, StartWorld, Interpolation);
    }

    return true;
}

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float3 StartWorld = Input.StartAndWidth.xyz;
    float3 EndWorld = Input.EndAndPadding.xyz;
    float4 StartClip = mul(float4(StartWorld, 1.0f), ViewProjection);
    float4 EndClip = mul(float4(EndWorld, 1.0f), ViewProjection);

    if (!ClipLineToNearPlane(StartClip, EndClip, StartWorld, EndWorld))
    {
        Output.Position = float4(0.0f, 0.0f, -1.0f, 1.0f);
        Output.Color = float4(Input.Color.rgb, 0.0f);
        Output.EdgeDistance = 0.0f;
        Output.HalfWidth = 0.0f;
        Output.WorldPosition = float3(0.0f, 0.0f, 0.0f);
        Output.GridSpacing = 0.0f;
        Output.ScreenNormal = float2(0.0f, 0.0f);
        Output.GridAxis = float2(0.0f, 0.0f);
        return Output;
    }

    float2 StartNDC = StartClip.xy / StartClip.w;
    float2 EndNDC = EndClip.xy / EndClip.w;

    float2 ScreenDirection = (EndNDC - StartNDC) * float2(Viewport.x, -Viewport.y);
    float ScreenLength = length(ScreenDirection);

    ScreenDirection = ScreenLength > 0.0001f ? ScreenDirection / ScreenLength : float2(1.0f, 0.0f);

    float2 ScreenNormal = float2(-ScreenDirection.y, ScreenDirection.x);

    float HalfWidth = max(Input.StartAndWidth.w * 0.5f, 0.5f);
    float RasterHalfWidth = HalfWidth + 1.0f;

    float2 OffsetPixels = ScreenNormal * Input.Corner.y * RasterHalfWidth;
    float2 OffsetNDC = OffsetPixels * float2(2.0f * Viewport.z, -2.0f * Viewport.w);

    float4 BaseClip = Input.Corner.x < 0.5f ? StartClip : EndClip;
    BaseClip.xy += OffsetNDC * BaseClip.w;

    Output.Position = BaseClip;
    Output.Color = Input.Color;
    Output.EdgeDistance = Input.Corner.y * RasterHalfWidth;
    Output.HalfWidth = HalfWidth;
    Output.WorldPosition = lerp(StartWorld, EndWorld, Input.Corner.x);
    Output.GridSpacing = Input.EndAndPadding.w;
    Output.ScreenNormal = ScreenNormal;
    Output.GridAxis = abs(Input.EndAndPadding.x - Input.StartAndWidth.x) < abs(Input.EndAndPadding.y - Input.StartAndWidth.y) ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float EffectiveHalfWidth = Input.HalfWidth;
    float4 FinalColor = Input.Color;
    if (Input.GridSpacing != 0.0f) {
        float Spacing = abs(Input.GridSpacing);
        float4 CurrentClip = mul(float4(Input.WorldPosition, 1.0f), ViewProjection);
        float4 AxisClip = mul(float4(Input.GridAxis, 0.0f, 0.0f), ViewProjection);
        float PixelSpacing = 0.0f;
        if (CurrentClip.w > 0.00001f) {
            float2 ScreenDelta = (AxisClip.xy - CurrentClip.xy * (AxisClip.w / CurrentClip.w)) * float2(Viewport.x, -Viewport.y) * (Spacing * 0.5f / CurrentClip.w);
            PixelSpacing = abs(dot(ScreenDelta, Input.ScreenNormal));
        }
        float Visibility = smoothstep(1.25f, 3.0f, PixelSpacing);
        FinalColor.a *= Visibility;
        if (Input.GridSpacing < 0.0f) {
            float ChildVisibility = smoothstep(1.25f, 3.0f, PixelSpacing * 0.1f);
            EffectiveHalfWidth = lerp(0.75f, 0.5f, ChildVisibility);
        }
        float GridDistance = length(Input.WorldPosition.xy - GridFade.xy);
        FinalColor.a *= 1.0f - smoothstep(GridFade.z, GridFade.w, GridDistance);
    }
    float Coverage = 1.0f - smoothstep(EffectiveHalfWidth - 0.5f, EffectiveHalfWidth + 0.5f, abs(Input.EdgeDistance));
    FinalColor.a *= Coverage;

    return FinalColor;
}
