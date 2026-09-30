#pragma once
#include "Math/FMath.h"

namespace PickingMath {
    using FBoxRaycaster = bool (*)(const DirectX::BoundingOrientedBox&, const FRay&, float&);
    extern FBoxRaycaster IntersectBox;
    void Initialize(bool ScalarBox = false);
    bool IntersectBoxScalar(const DirectX::BoundingOrientedBox& Box, const FRay& Ray, float& Distance);
    bool IntersectBoxSIMD(const DirectX::BoundingOrientedBox& Box, const FRay& Ray, float& Distance);
    __forceinline float ScalarSubtract(float A, float B) { return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(A), _mm_set_ss(B))); }
    __forceinline float ScalarMultiply(float A, float B) { return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(A), _mm_set_ss(B))); }
    __forceinline DirectX::XMFLOAT3 InverseRotateScalar(const DirectX::XMFLOAT3& V, const DirectX::XMFLOAT4& Q) {
        const float X = V.x * Q.w + (V.y * Q.z - V.z * Q.y), Y = -V.x * Q.z + (V.y * Q.w + V.z * Q.x);
        const float Z = V.x * Q.y + (-V.y * Q.x + V.z * Q.w), W = -V.x * Q.x + (-V.y * Q.y - V.z * Q.z);
        return {(Q.w * X - Q.x * W) + (-Q.y * Z + Q.z * Y), (Q.w * Y + Q.x * Z) + (-Q.y * W - Q.z * X), (Q.w * Z - Q.x * Y) + (Q.y * X - Q.z * W)};
    }
}
