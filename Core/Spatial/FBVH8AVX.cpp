#include "FBVH8Traversal.h"
#include "FBVH8Triangles.h"
#include "FBVH8TrianglePackets.h"
#include "FBVH8PacketSIMD.h"
#include <immintrin.h>

namespace {
    struct FPreparedRayAVX {
        __m256 Origin[3], Inverse[3];
        bool Parallel[3];
        Uint32 NearOffset[3], FarOffset[3];
        explicit __forceinline FPreparedRayAVX(const BVH8::FRayData& Ray) {
            const Uint32 Offsets[]{offsetof(BVH8::FNode, MinX), offsetof(BVH8::FNode, MinY), offsetof(BVH8::FNode, MinZ)};
            constexpr Uint32 SlabSize = sizeof(BVH8::FNode::MinX);
            for (Uint32 Axis = 0; Axis < 3; ++Axis) {
                Origin[Axis] = _mm256_set1_ps(Ray.Origin[Axis]);
                Inverse[Axis] = _mm256_set1_ps(Ray.InverseDirection[Axis]);
                Parallel[Axis] = Ray.Parallel[Axis];
                NearOffset[Axis] = Offsets[Axis] + (std::bit_cast<Uint32>(Ray.InverseDirection[Axis]) >> 31) * SlabSize;
                FarOffset[Axis] = NearOffset[Axis] ^ SlabSize;
            }
        }

        template<bool HasParallel> __forceinline Uint32 Intersect(const BVH8::FNode& Node, float MaxDistance, float* EntryDistances, Uint32 Count) const {
            const float* Min[]{Node.MinX, Node.MinY, Node.MinZ};
            const float* Max[]{Node.MaxX, Node.MaxY, Node.MaxZ};
            __m256 Near = _mm256_setzero_ps(), Far = _mm256_set1_ps(MaxDistance);
            __m256 Valid = _mm256_castsi256_ps(_mm256_set1_epi32(-1));
            for (Uint32 Axis = 0; Axis < 3; ++Axis) {
                if constexpr (!HasParallel) {
                    const auto* Bounds = reinterpret_cast<const std::byte*>(&Node);
                    const __m256 NearBound = _mm256_load_ps(reinterpret_cast<const float*>(Bounds + NearOffset[Axis]));
                    const __m256 FarBound = _mm256_load_ps(reinterpret_cast<const float*>(Bounds + FarOffset[Axis]));
                    Near = _mm256_max_ps(Near, _mm256_mul_ps(_mm256_sub_ps(NearBound, Origin[Axis]), Inverse[Axis]));
                    Far = _mm256_min_ps(Far, _mm256_mul_ps(_mm256_sub_ps(FarBound, Origin[Axis]), Inverse[Axis]));
                    continue;
                }
                const __m256 Lower = _mm256_load_ps(Min[Axis]), Upper = _mm256_load_ps(Max[Axis]);
                if constexpr (HasParallel) {
                    if (Parallel[Axis]) {
                        Valid = _mm256_and_ps(Valid, _mm256_and_ps(_mm256_cmp_ps(Origin[Axis], Lower, _CMP_GE_OQ), _mm256_cmp_ps(Origin[Axis], Upper, _CMP_LE_OQ)));
                        continue;
                    }
                }
                const __m256 A = _mm256_mul_ps(_mm256_sub_ps(Lower, Origin[Axis]), Inverse[Axis]), B = _mm256_mul_ps(_mm256_sub_ps(Upper, Origin[Axis]), Inverse[Axis]);
                Near = _mm256_max_ps(Near, _mm256_min_ps(A, B));
                Far = _mm256_min_ps(Far, _mm256_max_ps(A, B));
            }
            _mm256_storeu_ps(EntryDistances, Near);
            const __m256 Hits = HasParallel ? _mm256_and_ps(Valid, _mm256_cmp_ps(Near, Far, _CMP_LE_OQ)) : _mm256_cmp_ps(Near, Far, _CMP_LE_OQ);
            return static_cast<Uint32>(_mm256_movemask_ps(Hits)) & ((1u << Count) - 1);
        }
    };
}

Uint32 BVH8::IntersectAVX(const FNode& Node, const FRayData& Ray, float MaxDistance, float* EntryDistances, Uint32 Count) {
    return FPreparedRayAVX{Ray}.Intersect<true>(Node, MaxDistance, EntryDistances, Count);
}

bool BVH8::RaycastAVX(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf) {
    if (Ray.Parallel[0] || Ray.Parallel[1] || Ray.Parallel[2]) return Traverse<FPreparedRayAVX, true>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
    return Traverse<FPreparedRayAVX, false>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
}

bool BVH8::RaycastTrianglesAVX(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTriangle* Triangles, bool ReverseWinding) {
    const FTriangleVisitor Visitor{Triangles, Ray, ReverseWinding};
    if (RayData.Parallel[0] || RayData.Parallel[1] || RayData.Parallel[2]) return Traverse<FPreparedRayAVX, true>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
    return Traverse<FPreparedRayAVX, false>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
}

bool BVH8::RaycastTrianglePacketsAVX(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding) {
    const TTrianglePacketVisitor<FSIMD8> Visitor{Packets, Ray, ReverseWinding};
    if (RayData.Parallel[0] || RayData.Parallel[1] || RayData.Parallel[2]) return Traverse<FPreparedRayAVX, true>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
    return Traverse<FPreparedRayAVX, false>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
}
