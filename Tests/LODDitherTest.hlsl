#include "../Content/Shader/ModelResource.hlsli"

cbuffer TestConstants : register(b0) {
    float Dither;
    float3 Color;
};

float4 MainVS(uint Vertex : SV_VertexID) : SV_POSITION {
    return float4(Vertex == 2 ? 3.0f : -1.0f, Vertex == 1 ? 3.0f : -1.0f, 0.0f, 1.0f);
}

float4 MainPS(float4 Position : SV_POSITION) : SV_TARGET {
    ApplyLODDither(Position.xy, Dither);
    return float4(Color, 1.0f);
}
