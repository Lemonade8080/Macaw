#include "pch.h"
#include "FPickingMath.h"

PickingMath::FBoxRaycaster PickingMath::IntersectBox = PickingMath::IntersectBoxSIMD;
void PickingMath::Initialize(bool ScalarBox) { IntersectBox = ScalarBox ? IntersectBoxScalar : IntersectBoxSIMD; }
bool PickingMath::IntersectBoxSIMD(const DirectX::BoundingOrientedBox& Box, const FRay& Ray, float& Distance) { return Box.Intersects(Ray.position, Ray.direction, Distance); }

bool PickingMath::IntersectBoxScalar(const DirectX::BoundingOrientedBox& Box, const FRay& Ray, float& Distance) {
    const auto& Q = Box.Orientation;
    const float XX = Q.x * (Q.x + Q.x), YY = Q.y * (Q.y + Q.y), ZZ = Q.z * (Q.z + Q.z);
    const float XY = Q.x * (Q.y + Q.y), XZ = Q.x * (Q.z + Q.z), YZ = Q.y * (Q.z + Q.z);
    const float WX = Q.w * (Q.x + Q.x), WY = Q.w * (Q.y + Q.y), WZ = Q.w * (Q.z + Q.z);
    const float Axes[3][3]{{(1.0f - YY) - ZZ, XY + WZ, XZ - WY}, {XY - WZ, (1.0f - XX) - ZZ, YZ + WX}, {XZ + WY, YZ - WX, (1.0f - XX) - YY}};
    const float OX = Box.Center.x - Ray.position.x, OY = Box.Center.y - Ray.position.y, OZ = Box.Center.z - Ray.position.z;
    const float Extents[]{Box.Extents.x, Box.Extents.y, Box.Extents.z};
    float Near = -FLT_MAX, Far = FLT_MAX;
    Distance = 0.0f;
    #pragma loop(no_vector)
    for (Uint32 Axis = 0; Axis < 3; ++Axis) {
        const auto& A = Axes[Axis];
        const float O = (A[0] * OX + A[1] * OY) + A[2] * OZ, D = (A[0] * Ray.direction.x + A[1] * Ray.direction.y) + A[2] * Ray.direction.z;
        if (std::abs(D) <= 1e-20f) { if (std::abs(O) > Extents[Axis]) return false; continue; }
        const float Inverse = 1.0f / D, T1 = (O - Extents[Axis]) * Inverse, T2 = (O + Extents[Axis]) * Inverse;
        Near = (std::max)(Near, (std::min)(T1, T2)); Far = (std::min)(Far, (std::max)(T1, T2));
        if (Near > Far || Far < 0.0f) return false;
    }
    Distance = Near;
    return true;
}
