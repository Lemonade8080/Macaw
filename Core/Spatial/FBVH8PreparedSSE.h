#pragma once
#include "FBVH8.h"
#include <immintrin.h>
namespace BVH8 {
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

