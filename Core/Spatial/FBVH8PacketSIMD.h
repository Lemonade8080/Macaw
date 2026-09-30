#pragma once

#include <immintrin.h>

namespace BVH8 {
    struct FSIMD4 {
        using V = __m128;
        static constexpr Uint32 Width = 4;
        static __forceinline V Set(float X) { return _mm_set1_ps(X); }
        static __forceinline V Load(const float* P) { return _mm_load_ps(P); }
        static __forceinline V Add(V A, V B) { return _mm_add_ps(A, B); }
        static __forceinline V Sub(V A, V B) { return _mm_sub_ps(A, B); }
        static __forceinline V Mul(V A, V B) { return _mm_mul_ps(A, B); }
        static __forceinline V Div(V A, V B) { return _mm_div_ps(A, B); }
        static __forceinline V Xor(V A, V B) { return _mm_xor_ps(A, B); }
        static __forceinline V And(V A, V B) { return _mm_and_ps(A, B); }
        static __forceinline V GE(V A, V B) { return _mm_cmpge_ps(A, B); }
        static __forceinline V LE(V A, V B) { return _mm_cmple_ps(A, B); }
        static __forceinline V Select(V Mask, V Yes, V No) { return _mm_or_ps(_mm_and_ps(Mask, Yes), _mm_andnot_ps(Mask, No)); }
        static __forceinline Uint32 Bits(V A) { return static_cast<Uint32>(_mm_movemask_ps(A)); }
        static __forceinline V Mask(Uint32 M) { return _mm_castsi128_ps(_mm_setr_epi32(-(int)(M & 1), -(int)((M >> 1) & 1), -(int)((M >> 2) & 1), -(int)((M >> 3) & 1))); }
        static __forceinline float MinLane(V A) { A = _mm_min_ps(A, _mm_movehl_ps(A, A)); return _mm_cvtss_f32(_mm_min_ss(A, _mm_shuffle_ps(A, A, 1))); }
    };

#if defined(__AVX__)
    struct FSIMD8 {
        using V = __m256;
        static constexpr Uint32 Width = 8;
        static __forceinline V Set(float X) { return _mm256_set1_ps(X); }
        static __forceinline V Load(const float* P) { return _mm256_load_ps(P); }
        static __forceinline V Add(V A, V B) { return _mm256_add_ps(A, B); }
        static __forceinline V Sub(V A, V B) { return _mm256_sub_ps(A, B); }
        static __forceinline V Mul(V A, V B) { return _mm256_mul_ps(A, B); }
        static __forceinline V Div(V A, V B) { return _mm256_div_ps(A, B); }
        static __forceinline V Xor(V A, V B) { return _mm256_xor_ps(A, B); }
        static __forceinline V And(V A, V B) { return _mm256_and_ps(A, B); }
        static __forceinline V GE(V A, V B) { return _mm256_cmp_ps(A, B, _CMP_GE_OQ); }
        static __forceinline V LE(V A, V B) { return _mm256_cmp_ps(A, B, _CMP_LE_OQ); }
        static __forceinline V Select(V Mask, V Yes, V No) { return _mm256_blendv_ps(No, Yes, Mask); }
        static __forceinline Uint32 Bits(V A) { return static_cast<Uint32>(_mm256_movemask_ps(A)); }
        static __forceinline V Mask(Uint32 M) { return _mm256_castsi256_ps(_mm256_setr_epi32(-(int)(M & 1), -(int)((M >> 1) & 1), -(int)((M >> 2) & 1), -(int)((M >> 3) & 1), -(int)((M >> 4) & 1), -(int)((M >> 5) & 1), -(int)((M >> 6) & 1), -(int)((M >> 7) & 1))); }
        static __forceinline float MinLane(V A) { return FSIMD4::MinLane(_mm_min_ps(_mm256_castps256_ps128(A), _mm256_extractf128_ps(A, 1))); }
    };
#endif
}
