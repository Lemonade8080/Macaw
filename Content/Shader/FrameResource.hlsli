cbuffer FrameConstants : register(b0) {
    uint AnimationFrame;
    float AnimationTime;
    float2 FramePadding;
};

cbuffer ViewConstants : register(b1) {
    row_major float4x4 View;
    row_major float4x4 Projection;
    row_major float4x4 ViewProjection;
    row_major float4x4 CameraWorld;
    float4 Viewport;
    float4 GridFade;
    uint LightCount;
    float3 ViewPadding;
};
