#ifndef MACAW_MODEL_RESOURCE
#define MACAW_MODEL_RESOURCE

struct FMeshDrawRecord {
    uint mObjectIndex;
    uint mMaterialIndex;
    uint mFlags;
    float mLODDither;
};

struct FObjectTransform {
    row_major float4x4 mWorld;
};

struct FModelContext {
    row_major float4x4 mWorld;
    uint mMaterialIndex;
    uint mFlags;
    float mLODDither;
};

StructuredBuffer<FMeshDrawRecord> DrawRecords : register(t0);
StructuredBuffer<FObjectTransform> ObjectTransforms : register(t15);
StructuredBuffer<FObjectTransform> GizmoTransforms : register(t16);

FModelContext GetModelContext(uint DrawRecordIndex) {
    const FMeshDrawRecord Record = {DrawRecords[DrawRecordIndex]};
    FModelContext Result = {(float4x4)0.0f, 0u, 0u, 0.0f};

    if ((Record.mObjectIndex & 0x80000000u) != 0u) {
        Result.mWorld = GizmoTransforms[Record.mObjectIndex & 0x7fffffffu].mWorld;
    } else {
        Result.mWorld = ObjectTransforms[Record.mObjectIndex].mWorld;
    }

    Result.mMaterialIndex = Record.mMaterialIndex;
    Result.mFlags = Record.mFlags;
    Result.mLODDither = Record.mLODDither;

    return Result;
}

// Both LODs use the same screen-space pattern with complementary tests.
// No time-dependent noise: a stopped camera produces an identical image every frame.
void ApplyLODDither(float2 PixelPosition, float Dither) {
    [branch] if (Dither != 0.0f) {
        const float Noise = frac(52.9829189f * frac(dot(floor(PixelPosition), float2(0.06711056f, 0.00583715f))));
        if (Dither > 0.0f ? Noise < Dither : Noise >= -Dither) { discard; }
    }
}

#endif
