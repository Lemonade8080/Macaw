#include "pch.h"
#include "FBVH8Traversal.h"
#include "FBVH8TrianglePackets.h"
#include "FBVH8PacketSIMD.h"
#include <intrin.h>

BVH8::FRayData::FRayData(const FRay& Ray) {
    const float Direction[]{Ray.direction.x, Ray.direction.y, Ray.direction.z};
    Origin[0] = Ray.position.x; Origin[1] = Ray.position.y; Origin[2] = Ray.position.z;
    for (Uint32 Axis = 0; Axis < 3; ++Axis) {
        Parallel[Axis] = std::abs(Direction[Axis]) <= DirectX::XMVectorGetX(DirectX::g_RayEpsilon);
        InverseDirection[Axis] = Parallel[Axis] ? 1.0f : 1.0f / Direction[Axis];
    }
}

bool BVH8::SupportsAVX() {
    static const bool Supported = [] {
        int Features[4]{};
        __cpuid(Features, 1);
        constexpr int Required = (1 << 27) | (1 << 28);
        return (Features[2] & Required) == Required && (_xgetbv(0) & 6) == 6;
    }();
    return Supported;
}

namespace {
    BVH8::FRaycaster Raycaster = BVH8::RaycastSSE;
    BVH8::FTrianglePacketRaycaster TrianglePacketRaycaster = BVH8::RaycastTrianglePacketsSSE;
}

void BVH8::Initialize() {
    const bool AVX = SupportsAVX();
    Raycaster = AVX ? RaycastAVX : RaycastSSE;
    TrianglePacketRaycaster = AVX ? RaycastTrianglePacketsAVX : RaycastTrianglePacketsSSE;
}

bool BVH8::Raycast(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf) {
    return Raycaster(Nodes, RootReference, Ray, ClosestDistance, Context, VisitLeaf);
}

namespace {
    struct FPreparedRaySSE {
        __m128 Origin[3], Inverse[3];
        bool Parallel[3];
        explicit __forceinline FPreparedRaySSE(const BVH8::FRayData& Ray) {
            for (Uint32 Axis = 0; Axis < 3; ++Axis) {
                Origin[Axis] = _mm_set1_ps(Ray.Origin[Axis]);
                Inverse[Axis] = _mm_set1_ps(Ray.InverseDirection[Axis]);
                Parallel[Axis] = Ray.Parallel[Axis];
            }
        }

        template<bool HasParallel> __forceinline Uint32 Intersect(const BVH8::FNode& Node, float MaxDistance, float* EntryDistances, Uint32 Count) const {
            const float* Min[]{Node.MinX, Node.MinY, Node.MinZ};
            const float* Max[]{Node.MaxX, Node.MaxY, Node.MaxZ};
            Uint32 Mask = 0;
            for (Uint32 Base = 0; Base < Count; Base += 4) {
                __m128 Near = _mm_setzero_ps(), Far = _mm_set1_ps(MaxDistance);
                __m128 Valid = _mm_castsi128_ps(_mm_set1_epi32(-1));
                for (Uint32 Axis = 0; Axis < 3; ++Axis) {
                    const __m128 Lower = _mm_load_ps(Min[Axis] + Base), Upper = _mm_load_ps(Max[Axis] + Base);
                    if constexpr (HasParallel) {
                        if (Parallel[Axis]) {
                            Valid = _mm_and_ps(Valid, _mm_and_ps(_mm_cmpge_ps(Origin[Axis], Lower), _mm_cmple_ps(Origin[Axis], Upper)));
                            continue;
                        }
                    }
                    const __m128 A = _mm_mul_ps(_mm_sub_ps(Lower, Origin[Axis]), Inverse[Axis]), B = _mm_mul_ps(_mm_sub_ps(Upper, Origin[Axis]), Inverse[Axis]);
                    Near = _mm_max_ps(Near, _mm_min_ps(A, B));
                    Far = _mm_min_ps(Far, _mm_max_ps(A, B));
                }
                _mm_storeu_ps(EntryDistances + Base, Near);
                const __m128 Hits = HasParallel ? _mm_and_ps(Valid, _mm_cmple_ps(Near, Far)) : _mm_cmple_ps(Near, Far);
                Mask |= static_cast<Uint32>(_mm_movemask_ps(Hits)) << Base;
            }
            return Mask & ((1u << Count) - 1);
        }
    };
}

bool BVH8::RaycastSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf) {
    if (Ray.Parallel[0] || Ray.Parallel[1] || Ray.Parallel[2]) return Traverse<FPreparedRaySSE, true>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
    return Traverse<FPreparedRaySSE, false>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
}

bool BVH8::RaycastTrianglePacketsSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding) {
    const TTrianglePacketVisitor<FSIMD4> Visitor{Packets, Ray, ReverseWinding};
    if (RayData.Parallel[0] || RayData.Parallel[1] || RayData.Parallel[2]) return Traverse<FPreparedRaySSE, true>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
    return Traverse<FPreparedRaySSE, false>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
}

bool BVH8::RaycastTrianglePackets(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding) {
    return TrianglePacketRaycaster(Nodes, RootReference, RayData, Ray, ClosestDistance, Packets, ReverseWinding);
}
