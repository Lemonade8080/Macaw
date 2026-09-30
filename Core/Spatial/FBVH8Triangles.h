#pragma once

#include "FBVH8.h"

namespace BVH8 {
    struct FTriangle {
        DirectX::XMFLOAT3 V0, Edge1, Edge2;
    };
    static_assert(sizeof(FTriangle) == 36);

    struct FTriangleVisitor {
        const FTriangle* Triangles;
        DirectX::XMVECTOR Origin, Direction;
        bool ReverseWinding;
        FTriangleVisitor(const FTriangle* InTriangles, const FRay& Ray, bool InReverseWinding = false) : Triangles(InTriangles), Origin(Ray.position), Direction(Ray.direction), ReverseWinding(InReverseWinding) {}

        __forceinline bool operator()(Uint32 Index, Uint32, float& Limit) const {
            using namespace DirectX;
            const auto& Triangle = Triangles[Index];
            const XMVECTOR Edge1 = XMLoadFloat3(&Triangle.Edge1), Edge2 = XMLoadFloat3(&Triangle.Edge2);
            const XMVECTOR P = XMVector3Cross(Direction, Edge2), Det = XMVector3Dot(Edge1, P);
            const bool Front = XMVector3GreaterOrEqual(Det, g_RayEpsilon);
            if (ReverseWinding ? !XMVector3LessOrEqual(Det, g_RayNegEpsilon) : !Front) return false;
            const XMVECTOR S = XMVectorSubtract(Origin, XMLoadFloat3(&Triangle.V0)), Q = XMVector3Cross(S, Edge1);
            const XMVECTOR U = XMVector3Dot(S, P), V = XMVector3Dot(Direction, Q), T = XMVector3Dot(Edge2, Q);
            const XMVECTOR Zero = XMVectorZero(), UV = XMVectorAdd(U, V);
            XMVECTOR Reject;
            if (Front) {
                Reject = XMVectorOrInt(XMVectorLess(U, Zero), XMVectorGreater(U, Det));
                Reject = XMVectorOrInt(Reject, XMVectorOrInt(XMVectorLess(V, Zero), XMVectorGreater(UV, Det)));
                Reject = XMVectorOrInt(Reject, XMVectorLess(T, Zero));
            } else {
                Reject = XMVectorOrInt(XMVectorGreater(U, Zero), XMVectorLess(U, Det));
                Reject = XMVectorOrInt(Reject, XMVectorOrInt(XMVectorGreater(V, Zero), XMVectorLess(UV, Det)));
                Reject = XMVectorOrInt(Reject, XMVectorGreater(T, Zero));
            }
            if (XMVector4EqualInt(Reject, XMVectorTrueInt())) return false;
            const float Distance = XMVectorGetX(XMVectorDivide(T, Det));
            if (!(Distance <= Limit)) return false;
            Limit = Distance;
            return true;
        }
    };
}
