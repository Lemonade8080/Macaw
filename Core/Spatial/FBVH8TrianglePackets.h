#pragma once

#include "FBVH8.h"

namespace BVH8 {
    struct alignas(32) FTrianglePacket {
        float Edge1[3][8]{}, Edge2[3][8]{}, V0[3][8]{};
    };
    static_assert(sizeof(FTrianglePacket) == 288);

    template<class S> struct TTrianglePacketVisitor {
        using V = typename S::V;
        const FTrianglePacket* Packets;
        V Origin[3], Direction[3], Sign;
        TTrianglePacketVisitor(const FTrianglePacket* InPackets, const FRay& Ray, bool ReverseWinding) : Packets(InPackets), Sign(S::Set(ReverseWinding ? -0.0f : 0.0f)) {
            const float O[]{Ray.position.x, Ray.position.y, Ray.position.z}, D[]{Ray.direction.x, Ray.direction.y, Ray.direction.z};
            for (Uint32 Axis = 0; Axis < 3; ++Axis) { Origin[Axis] = S::Set(O[Axis]); Direction[Axis] = S::Set(D[Axis]); }
        }
        static __forceinline V Dot(V AX, V AY, V AZ, V BX, V BY, V BZ) { return S::Add(S::Add(S::Mul(AX, BX), S::Mul(AY, BY)), S::Mul(AZ, BZ)); }
        __forceinline bool operator()(Uint32 Index, Uint32 Count, float& Limit) const {
            const auto& Packet = Packets[Index];
            bool Hit = false;
            for (Uint32 Base = 0; Base < Count; Base += S::Width) {
                const Uint32 Active = (((1u << Count) - 1) >> Base) & ((1u << S::Width) - 1);
                const V E1X = S::Load(Packet.Edge1[0] + Base), E1Y = S::Load(Packet.Edge1[1] + Base), E1Z = S::Load(Packet.Edge1[2] + Base);
                const V E2X = S::Load(Packet.Edge2[0] + Base), E2Y = S::Load(Packet.Edge2[1] + Base), E2Z = S::Load(Packet.Edge2[2] + Base);
                const V PX = S::Sub(S::Mul(Direction[1], E2Z), S::Mul(Direction[2], E2Y));
                const V PY = S::Sub(S::Mul(Direction[2], E2X), S::Mul(Direction[0], E2Z));
                const V PZ = S::Sub(S::Mul(Direction[0], E2Y), S::Mul(Direction[1], E2X));
                const V Det = S::Xor(Dot(E1X, E1Y, E1Z, PX, PY, PZ), Sign);
                V Valid = S::GE(Det, S::Set(DirectX::XMVectorGetX(DirectX::g_RayEpsilon)));
                if ((S::Bits(Valid) & Active) == 0) continue;
                const V SX = S::Sub(Origin[0], S::Load(Packet.V0[0] + Base)), SY = S::Sub(Origin[1], S::Load(Packet.V0[1] + Base)), SZ = S::Sub(Origin[2], S::Load(Packet.V0[2] + Base));
                const V QX = S::Sub(S::Mul(SY, E1Z), S::Mul(SZ, E1Y));
                const V QY = S::Sub(S::Mul(SZ, E1X), S::Mul(SX, E1Z));
                const V QZ = S::Sub(S::Mul(SX, E1Y), S::Mul(SY, E1X));
                const V U = S::Xor(Dot(SX, SY, SZ, PX, PY, PZ), Sign), B = S::Xor(Dot(Direction[0], Direction[1], Direction[2], QX, QY, QZ), Sign);
                const V T = S::Xor(Dot(E2X, E2Y, E2Z, QX, QY, QZ), Sign), Zero = S::Set(0.0f);
                Valid = S::And(Valid, S::And(S::GE(U, Zero), S::GE(B, Zero)));
                Valid = S::And(Valid, S::And(S::LE(S::Add(U, B), Det), S::GE(T, Zero)));
                if ((S::Bits(Valid) & Active) == 0) continue;
                const V Distance = S::Div(T, S::Select(Valid, Det, S::Set(1.0f)));
                Valid = S::And(Valid, S::LE(Distance, S::Set(Limit)));
                const Uint32 Hits = S::Bits(Valid) & Active;
                if (Hits == 0) continue;
                Limit = S::MinLane(S::Select(S::Mask(Hits), Distance, S::Set(Limit)));
                Hit = true;
            }
            return Hit;
        }
    };
}
