#pragma once
#include "FBVH8Traversal.h"
#include "FBVH8TrianglePackets.h"

namespace BVH8 {
    struct FPreparedRayScalar {
        FRayData Ray;
        Uint32 NearOffset[3], FarOffset[3];
        explicit FPreparedRayScalar(const FRayData& InRay) : Ray(InRay) {
            const Uint32 Offsets[]{offsetof(FNode, MinX), offsetof(FNode, MinY), offsetof(FNode, MinZ)};
            for (Uint32 Axis = 0; Axis < 3; ++Axis) { NearOffset[Axis] = Offsets[Axis] + (std::bit_cast<Uint32>(Ray.InverseDirection[Axis]) >> 31) * 32; FarOffset[Axis] = NearOffset[Axis] ^ 32; }
        }
        template<bool HasParallel> __forceinline Uint32 Intersect(const FNode& Node, float Limit, float* Distances, Uint32 Count) const {
            const float* Min[]{Node.MinX, Node.MinY, Node.MinZ};
            const float* Max[]{Node.MaxX, Node.MaxY, Node.MaxZ};
            const auto* Bounds = reinterpret_cast<const std::byte*>(&Node);
            Uint32 Mask = 0;
            #pragma loop(no_vector)
            for (Uint32 Lane = 0; Lane < Count; ++Lane) {
                float Near = 0.0f, Far = Limit;
                bool Valid = true;
                for (Uint32 Axis = 0; Axis < 3; ++Axis) {
                    if constexpr (HasParallel) {
                        if (Ray.Parallel[Axis]) { if (Ray.Origin[Axis] < Min[Axis][Lane] || Ray.Origin[Axis] > Max[Axis][Lane]) { Valid = false; break; } continue; }
                    }
                    const float A = (reinterpret_cast<const float*>(Bounds + NearOffset[Axis])[Lane] - Ray.Origin[Axis]) * Ray.InverseDirection[Axis];
                    const float B = (reinterpret_cast<const float*>(Bounds + FarOffset[Axis])[Lane] - Ray.Origin[Axis]) * Ray.InverseDirection[Axis];
                    Near = (std::max)(Near, A); Far = (std::min)(Far, B);
                    if (Near > Far) { Valid = false; break; }
                }
                Distances[Lane] = Near;
                if (Valid && Near <= Far) Mask |= 1u << Lane;
            }
            return Mask;
        }
    };

    struct FScalarTrianglePacketVisitor {
        const FTrianglePacket* Packets;
        float O[3], D[3];
        bool Reverse;
        FScalarTrianglePacketVisitor(const FTrianglePacket* InPackets, const FRay& Ray, bool ReverseWinding) : Packets(InPackets), O{Ray.position.x, Ray.position.y, Ray.position.z}, D{Ray.direction.x, Ray.direction.y, Ray.direction.z}, Reverse(ReverseWinding) {}
        __forceinline bool operator()(Uint32 Index, Uint32 Count, float& Limit) const {
            const auto& P = Packets[Index];
            bool Hit = false;
            #pragma loop(no_vector)
            for (Uint32 i = 0; i < Count; ++i) {
                const float X = D[1] * P.Edge2[2][i] - D[2] * P.Edge2[1][i], Y = D[2] * P.Edge2[0][i] - D[0] * P.Edge2[2][i], Z = D[0] * P.Edge2[1][i] - D[1] * P.Edge2[0][i];
                float Det = (P.Edge1[0][i] * X + P.Edge1[1][i] * Y) + P.Edge1[2][i] * Z;
                if (Reverse) Det = -Det;
                if (!(Det >= 1e-20f)) continue;
                const float SX = O[0] - P.V0[0][i], SY = O[1] - P.V0[1][i], SZ = O[2] - P.V0[2][i];
                float U = (SX * X + SY * Y) + SZ * Z;
                if (Reverse) U = -U;
                if (!(U >= 0.0f && U <= Det)) continue;
                const float QX = SY * P.Edge1[2][i] - SZ * P.Edge1[1][i], QY = SZ * P.Edge1[0][i] - SX * P.Edge1[2][i], QZ = SX * P.Edge1[1][i] - SY * P.Edge1[0][i];
                float V = (D[0] * QX + D[1] * QY) + D[2] * QZ, T = (P.Edge2[0][i] * QX + P.Edge2[1][i] * QY) + P.Edge2[2][i] * QZ;
                if (Reverse) { V = -V; T = -T; }
                if (!(V >= 0.0f && U + V <= Det && T >= 0.0f)) continue;
                const float Distance = T / Det;
                if (Distance <= Limit) { Limit = Distance; Hit = true; }
            }
            return Hit;
        }
    };

    template<class TPrepared, class TVisitor> bool CastPackets(const FNode* Nodes, Uint32 Root, const FRayData& Data, const FRay& Ray, float& Limit, const FTrianglePacket* Packets, bool Reverse) {
        const TVisitor Visitor{Packets, Ray, Reverse};
        if (Data.Parallel[0] || Data.Parallel[1] || Data.Parallel[2]) return Traverse<TPrepared, true>(Nodes, Root, Data, Limit, Visitor);
        return Traverse<TPrepared, false>(Nodes, Root, Data, Limit, Visitor);
    }
}
