struct FModelContext
{
    row_major float4x4 World;
    uint MaterialIndex;
    uint Flags;
};

struct FMaterial
{
    float4 BaseColor;

    // Paddings
    float4 Parameters0;
    float4 Parameters1;
    float4 Parameters2;
    float4 Parameters3;
    float4 Parameters4;
    float4 Parameters5;
    float4 Parameters6;
};

StructuredBuffer<FModelContext> ModelContexts : register(t0); // ModelContext[] 
StructuredBuffer<FMaterial> MaterialBuffer : register(t1);
#include "Lighting.hlsli"

Texture2D BaseColorTexture : register(t4);
Texture2D VATTexture : register(t6);

SamplerState LinearWrap : register(s0);
SamplerState PointClamp : register(s2);

#include "FrameResource.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
    float3 WorldPosition : TEXCOORD1;
    nointerpolation uint MaterialIndex : Jungle1;
    nointerpolation float3 ColorCoefficient : Jungle2;
};

PS_INPUT mainVS(VS_INPUT Input, uint ModelIndex : MODEL_INDEX)
{
    PS_INPUT Output;

    //이전 로컬    
    float3 MinBound = float3(-119.5014, -0.7041, -103.2716);
    float3 MaxBound = float3(142.2604, 181.1934, 90.4448);

    float3 BoundsSize = MaxBound - MinBound;
           
    
    float TotalFrame = 250.0;

    //버텍스 컬러에 0 ~ 1로 저장된 인덱스 값
    float IndexNormalize = Input.Color.r;

    //프레임에 따라 y값 산출
    float FrameNormalize = AnimationFrame / (TotalFrame - 1);

    //VAT 텍스처 샘플링 할 uv
    float2 VATUV = float2(IndexNormalize, FrameNormalize);   
    
    
    //VATTexture에서 버텍스의 위치값을 산출한다.
    //float3 VATPosition = VATTexture.Sample(LinearWrap, VATUV).rgb;

    //그냥 Sample()은 픽셀 셰이더에서 밉맵 레벨을 자동 계산하는데 그건 버텍스 셰이더에는 없다.
    //SampleLevel()로 밉맵을 직접 지정한다.
    float3 VATPositionNomalized = VATTexture.SampleLevel(PointClamp, VATUV, 0).rgb;

    float3 BlenderLocalPos = VATPositionNomalized * BoundsSize + MinBound;

    //좌표계 변환
    float3 VATPosition = float3(-BlenderLocalPos.z, BlenderLocalPos.x, BlenderLocalPos.y);
    
    
    FModelContext ModelContext = ModelContexts[ModelIndex];

    //VAT에서 뽑은 위치값에 World 행렬 곱해주기
    float4 WorldPosition = mul(float4(VATPosition, 1.0f), ModelContext.World);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Normal = mul(Input.Normal, (float3x3) ModelContext.World);
    Output.UV = Input.UV;
    Output.Color = Input.Color;
    Output.WorldPosition = WorldPosition.xyz;
    Output.MaterialIndex = ModelContext.MaterialIndex;
    Output.ColorCoefficient = float3(1.0f, 1.0f, 1.0f);

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 Color = BaseColorTexture.Sample(LinearWrap, Input.UV);
    return Color;
 

}
