#ifndef MACAW_MODEL_RESOURCE
#define MACAW_MODEL_RESOURCE

struct FMeshDrawRecord {
    uint mObjectIndex;
    uint mMaterialIndex;
    uint mFlags;
    uint mPadding;
};

struct FObjectTransform {
    row_major float4x4 mWorld;
};

struct FModelContext {
    row_major float4x4 mWorld;
    uint mMaterialIndex;
    uint mFlags;
};

StructuredBuffer<FMeshDrawRecord> DrawRecords : register(t0);
StructuredBuffer<FObjectTransform> ObjectTransforms : register(t15);
StructuredBuffer<FObjectTransform> GizmoTransforms : register(t16);

FModelContext GetModelContext(uint DrawRecordIndex) {
    const FMeshDrawRecord Record = {DrawRecords[DrawRecordIndex]};
    FModelContext Result = {(float4x4)0.0f, 0u, 0u};

    if ((Record.mObjectIndex & 0x80000000u) != 0u) {
        Result.mWorld = GizmoTransforms[Record.mObjectIndex & 0x7fffffffu].mWorld;
    } else {
        Result.mWorld = ObjectTransforms[Record.mObjectIndex].mWorld;
    }

    Result.mMaterialIndex = Record.mMaterialIndex;
    Result.mFlags = Record.mFlags;

    return Result;
}

#endif
