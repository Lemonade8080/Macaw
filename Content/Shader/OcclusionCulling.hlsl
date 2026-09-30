struct FBounds {
    float4 mCenter;
    float4 mExtents;
};

struct FMeshDrawRecord {
    uint mObjectIndex;
    uint mMaterialIndex;
    uint mFlags;
    uint mPadding;
};

struct FBatch {
    uint mFirstRecord;
    uint mRecordCount;
    uint mIndexCount;
    uint mFirstIndex;
};

cbuffer FOcclusionConstants : register(b0) {
    row_major float4x4 ViewProjection;
    uint SourceWidth;
    uint SourceHeight;
    uint DestinationWidth;
    uint DestinationHeight;
    uint ObjectCount;
    uint RecordCount;
    uint BatchCount;
    uint MipCount;
    uint DispatchWidth;
    float DepthBias;
    uint CullingPass;
    uint Padding;
};

Texture2D<float> Depth : register(t0);
StructuredBuffer<FBounds> Bounds : register(t1);
StructuredBuffer<FMeshDrawRecord> InputRecords : register(t2);
StructuredBuffer<uint> Visibility : register(t3);
StructuredBuffer<FBatch> Batches : register(t4);
RWTexture2D<float> OutputDepth : register(u0);
RWStructuredBuffer<uint> OutputVisibility : register(u0);
RWStructuredBuffer<FMeshDrawRecord> OutputRecords : register(u1);
RWByteAddressBuffer Arguments : register(u2);
groupshared uint VisibleCount;
groupshared uint PrefixCounts[64];

[numthreads(8, 8, 1)]
void CopyDepth(uint3 Thread : SV_DispatchThreadID) {
    if (Thread.x >= DestinationWidth || Thread.y >= DestinationHeight) {
        return;
    }
    float Value = {1.0f};
    if (Thread.x < SourceWidth && Thread.y < SourceHeight) {
        Value = Depth.Load(int3(Thread.xy, 0));
    }
    OutputDepth[Thread.xy] = Value;
}

[numthreads(8, 8, 1)]
void ReduceDepth(uint3 Thread : SV_DispatchThreadID) {
    if (Thread.x >= DestinationWidth || Thread.y >= DestinationHeight) {
        return;
    }
    uint2 Source = {Thread.xy * 2};
    uint2 Limit = {SourceWidth - 1u, SourceHeight - 1u};
    float Maximum = {Depth.Load(int3(min(Source, Limit), 0))};
    Maximum = max(Maximum, Depth.Load(int3(min(Source + uint2(1, 0), Limit), 0)));
    Maximum = max(Maximum, Depth.Load(int3(min(Source + uint2(0, 1), Limit), 0)));
    Maximum = max(Maximum, Depth.Load(int3(min(Source + uint2(1, 1), Limit), 0)));
    OutputDepth[Thread.xy] = Maximum;
}

[numthreads(64, 1, 1)]
void CullObjects(uint3 Group : SV_GroupID, uint Thread : SV_GroupIndex) {
    uint ObjectIndex = {(Group.x + Group.y * DispatchWidth) * 64 + Thread};
    if (ObjectIndex >= ObjectCount) {
        return;
    }
    if (CullingPass != 0 && OutputVisibility[ObjectIndex] != 0) {
        return;
    }
    uint Visible = {CullingPass == 0 ? 1u : 2u};
    OutputVisibility[ObjectIndex] = Visible;
    FBounds Box = {Bounds[ObjectIndex]};
    if (Box.mCenter.w == 0.0f) {
        return;
    }
    float2 Minimum = {1.0e20f, 1.0e20f};
    float2 Maximum = {-1.0e20f, -1.0e20f};
    float NearestDepth = {1.0f};
    [unroll]
    for (uint Corner = {0}; Corner < 8; ++Corner) {
        float3 Sign = {(Corner & 1) != 0 ? 1.0f : -1.0f, (Corner & 2) != 0 ? 1.0f : -1.0f, (Corner & 4) != 0 ? 1.0f : -1.0f};
        float3 Position = {Box.mCenter.xyz + Sign * Box.mExtents.xyz};
        float4 WorldPosition = {Position, 1.0f};
        float4 Clip = {mul(WorldPosition, ViewProjection)};
        if (!all(isfinite(Clip)) || Clip.w <= 0.00001f || Clip.z <= 0.00001f) {
            return;
        }
        float3 Ndc = {Clip.xyz / Clip.w};
        if (!all(isfinite(Ndc))) {
            return;
        }
        float2 Pixel = {(Ndc.x * 0.5f + 0.5f) * SourceWidth, (0.5f - Ndc.y * 0.5f) * SourceHeight};
        Minimum = min(Minimum, Pixel);
        Maximum = max(Maximum, Pixel);
        NearestDepth = min(NearestDepth, Ndc.z);
    }
    if (Maximum.x < 0.0f || Maximum.y < 0.0f || Minimum.x >= (float)SourceWidth || Minimum.y >= (float)SourceHeight) {
        return;
    }
    float2 Limit = {SourceWidth - 1u, SourceHeight - 1u};
    uint2 FirstPixel = {(uint2)clamp(floor(Minimum) - 1.0f, 0.0f, Limit)};
    uint2 LastPixel = {(uint2)clamp(ceil(Maximum) + 1.0f, 0.0f, Limit)};
    uint Size = {max(LastPixel.x - FirstPixel.x + 1u, LastPixel.y - FirstPixel.y + 1u)};
    uint Mip = {min((uint)ceil(log2((float)Size)), MipCount - 1u)};
    uint2 FirstTexel = {FirstPixel >> Mip};
    uint2 LastTexel = {LastPixel >> Mip};
    float FarthestDepth = {0.0f};
    for (uint Y = {FirstTexel.y}; Y <= LastTexel.y; ++Y) {
        for (uint X = {FirstTexel.x}; X <= LastTexel.x; ++X) {
            FarthestDepth = max(FarthestDepth, Depth.Load(int3(X, Y, Mip)));
        }
    }
    OutputVisibility[ObjectIndex] = NearestDepth <= FarthestDepth + DepthBias ? Visible : 0;
}

void StoreArguments(uint Index, FBatch Batch, uint Count, uint FirstRecord) {
    uint Offset = {Index * 20};
    Arguments.Store(Offset, Batch.mIndexCount);
    Arguments.Store(Offset + 4, Count);
    Arguments.Store(Offset + 8, Batch.mFirstIndex);
    Arguments.Store(Offset + 12, 0);
    Arguments.Store(Offset + 16, FirstRecord);
}

[numthreads(64, 1, 1)]
void BuildObjectArguments(uint3 Group : SV_GroupID, uint Thread : SV_GroupIndex) {
    uint RecordIndex = {(Group.x + Group.y * DispatchWidth) * 64 + Thread};
    if (RecordIndex >= RecordCount) {
        return;
    }
    uint First = {0};
    uint Last = {BatchCount};
    while (First + 1 < Last) {
        uint Middle = {(First + Last) / 2};
        if (Batches[Middle].mFirstRecord <= RecordIndex) {
            First = Middle;
        } else {
            Last = Middle;
        }
    }
    uint BatchIndex = {First};
    FBatch Batch = {Batches[BatchIndex]};
    FMeshDrawRecord Record = {InputRecords[RecordIndex]};
    uint Count = {Visibility[Record.mObjectIndex] == (CullingPass == 0 ? 1u : 2u) ? 1u : 0u};
    StoreArguments(RecordIndex, Batch, Count, RecordIndex);
}

[numthreads(64, 1, 1)]
void BuildInstancedArguments(uint3 Group : SV_GroupID, uint Thread : SV_GroupIndex) {
    uint BatchIndex = {Group.x + Group.y * DispatchWidth};
    if (BatchIndex >= BatchCount) {
        return;
    }
    if (Thread == 0) {
        VisibleCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    FBatch Batch = {Batches[BatchIndex]};
    for (uint Chunk = {0}; Chunk < Batch.mRecordCount; Chunk += 64) {
        uint Index = {Chunk + Thread};
        FMeshDrawRecord Record = {0, 0, 0, 0};
        uint Visible = {0};
        if (Index < Batch.mRecordCount) {
            Record = InputRecords[Batch.mFirstRecord + Index];
            Visible = Visibility[Record.mObjectIndex] == (CullingPass == 0 ? 1u : 2u) ? 1u : 0u;
        }
        PrefixCounts[Thread] = Visible;
        GroupMemoryBarrierWithGroupSync();
        [unroll]
        for (uint Step = {1}; Step < 64; Step *= 2) {
            uint Addend = {0};
            if (Thread >= Step) {
                Addend = PrefixCounts[Thread - Step];
            }
            GroupMemoryBarrierWithGroupSync();
            PrefixCounts[Thread] += Addend;
            GroupMemoryBarrierWithGroupSync();
        }
        uint OutputBase = {VisibleCount};
        if (Visible != 0) {
            OutputRecords[Batch.mFirstRecord + OutputBase + PrefixCounts[Thread] - 1] = Record;
        }
        GroupMemoryBarrierWithGroupSync();
        if (Thread == 0) {
            VisibleCount += PrefixCounts[63];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    GroupMemoryBarrierWithGroupSync();
    if (Thread == 0) {
        StoreArguments(BatchIndex, Batch, VisibleCount, Batch.mFirstRecord);
    }
}
