#include "pch.h"
#include "FBVH8Traversal.h"
#include "FBVH8Triangles.h"
#include "FBVH8TrianglePackets.h"
#include "FBVH8PacketSIMD.h"
#include <intrin.h>
#include "FBVH8PreparedSSE.h"
#include "FBVH8Scalar.h"

DirectX::BoundingBox FBVH8::RefitChild(Uint32 Reference, Uint32 Lane, const DirectX::BoundingBox& Box) {
    auto& Node = mNodes.at(Reference & BVH8::IndexMask);
    const Uint32 Count = BVH8::GetCount(Reference);
    if (Lane >= Count) throw std::out_of_range("BVH8 child slot");
    float* Lower[]{Node.MinX, Node.MinY, Node.MinZ};
    float* Upper[]{Node.MaxX, Node.MaxY, Node.MaxZ};
    const float Center[]{Box.Center.x, Box.Center.y, Box.Center.z}, Extents[]{Box.Extents.x, Box.Extents.y, Box.Extents.z};
    float ResultCenter[3], ResultExtents[3];
    for (Uint32 Axis = 0; Axis < 3; ++Axis) {
        Lower[Axis][Lane] = std::nextafter(Center[Axis] - Extents[Axis], -std::numeric_limits<float>::infinity());
        Upper[Axis][Lane] = std::nextafter(Center[Axis] + Extents[Axis], std::numeric_limits<float>::infinity());
        float Min = Lower[Axis][0], Max = Upper[Axis][0];
        for (Uint32 i = 1; i < Count; ++i) { Min = (std::min)(Min, Lower[Axis][i]); Max = (std::max)(Max, Upper[Axis][i]); }
        ResultCenter[Axis] = Min * 0.5f + Max * 0.5f;
        ResultExtents[Axis] = (std::max)(ResultCenter[Axis] - Min, Max - ResultCenter[Axis]);
    }
    return {{ResultCenter[0], ResultCenter[1], ResultCenter[2]}, {ResultExtents[0], ResultExtents[1], ResultExtents[2]}};
}

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
    BVH8::FTriangleRaycaster TriangleRaycaster = BVH8::RaycastTrianglesSSE;
    BVH8::FTrianglePacketRaycaster TrianglePacketRaycaster = BVH8::RaycastTrianglePacketsSSE;
}

void BVH8::Initialize(EKernel World, EKernel Mesh, EKernel Triangles) {
    const auto Resolve = [](EKernel Kernel) { return Kernel == EKernel::Auto ? (SupportsAVX() ? EKernel::AVX : EKernel::SSE) : Kernel == EKernel::AVX && !SupportsAVX() ? EKernel::SSE : Kernel; };
    World = Resolve(World); Mesh = Resolve(Mesh); Triangles = Resolve(Triangles);
    Raycaster = World == EKernel::Scalar ? RaycastScalar : World == EKernel::SSE ? RaycastSSE : RaycastAVX;
    TriangleRaycaster = SupportsAVX() ? RaycastTrianglesAVX : RaycastTrianglesSSE;
    TrianglePacketRaycaster = Mesh == EKernel::AVX || Triangles == EKernel::AVX ? SelectPacketAVX(Mesh, Triangles) : SelectPacketBaseline(Mesh, Triangles);
}

bool BVH8::Raycast(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf) {
    return Raycaster(Nodes, RootReference, Ray, ClosestDistance, Context, VisitLeaf);
}

Uint32 BVH8::IntersectSSE(const FNode& Node, const FRayData& Ray, float MaxDistance, float* EntryDistances, Uint32 Count) {
    return FPreparedRaySSE{Ray}.Intersect<true>(Node, MaxDistance, EntryDistances, Count);
}

bool BVH8::RaycastSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf) {
    if (Ray.Parallel[0] || Ray.Parallel[1] || Ray.Parallel[2]) return Traverse<FPreparedRaySSE, true>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
    return Traverse<FPreparedRaySSE, false>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
}

bool BVH8::RaycastTrianglesSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTriangle* Triangles, bool ReverseWinding) {
    const FTriangleVisitor Visitor{Triangles, Ray, ReverseWinding};
    if (RayData.Parallel[0] || RayData.Parallel[1] || RayData.Parallel[2]) return Traverse<FPreparedRaySSE, true>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
    return Traverse<FPreparedRaySSE, false>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
}

bool BVH8::RaycastTriangles(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTriangle* Triangles, bool ReverseWinding) {
    return TriangleRaycaster(Nodes, RootReference, RayData, Ray, ClosestDistance, Triangles, ReverseWinding);
}

bool BVH8::RaycastTrianglePacketsSSE(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding) {
    const TTrianglePacketVisitor<FSIMD4> Visitor{Packets, Ray, ReverseWinding};
    if (RayData.Parallel[0] || RayData.Parallel[1] || RayData.Parallel[2]) return Traverse<FPreparedRaySSE, true>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
    return Traverse<FPreparedRaySSE, false>(Nodes, RootReference, RayData, ClosestDistance, Visitor);
}

bool BVH8::RaycastTrianglePackets(const FNode* Nodes, Uint32 RootReference, const FRayData& RayData, const FRay& Ray, float& ClosestDistance, const FTrianglePacket* Packets, bool ReverseWinding) {
    return TrianglePacketRaycaster(Nodes, RootReference, RayData, Ray, ClosestDistance, Packets, ReverseWinding);
}

bool BVH8::RaycastScalar(const FNode* Nodes, Uint32 RootReference, const FRayData& Ray, float& ClosestDistance, void* Context, FVisitLeaf VisitLeaf) {
    if (Ray.Parallel[0] || Ray.Parallel[1] || Ray.Parallel[2]) return Traverse<FPreparedRayScalar, true>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
    return Traverse<FPreparedRayScalar, false>(Nodes, RootReference, Ray, ClosestDistance, [&](Uint32 Leaf, Uint32, float& Distance) { return VisitLeaf(Context, Leaf, Distance); });
}

Uint32 BVH8::IntersectScalar(const FNode& Node, const FRayData& Ray, float MaxDistance, float* EntryDistances, Uint32 Count) { return FPreparedRayScalar{Ray}.Intersect<true>(Node, MaxDistance, EntryDistances, Count); }

BVH8::FTrianglePacketRaycaster BVH8::SelectPacketBaseline(EKernel Nodes, EKernel Triangles) {
    if (Nodes == EKernel::Scalar) return Triangles == EKernel::Scalar ? CastPackets<FPreparedRayScalar, FScalarTrianglePacketVisitor> : CastPackets<FPreparedRayScalar, TTrianglePacketVisitor<FSIMD4>>;
    return Triangles == EKernel::Scalar ? CastPackets<FPreparedRaySSE, FScalarTrianglePacketVisitor> : RaycastTrianglePacketsSSE;
}
