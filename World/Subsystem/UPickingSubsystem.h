#pragma once

#include "UWorldSubsystem.h"

#include "Core/Base/TObjectRef.h"
#include "Core/Spatial/FDynamicBVH8.h"
#include "World/Component/UPrimitiveComponent.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include "World/Component/FMeshPickingProxy.h"

class FWorldRaycastAccelerationStructure {
private:
    static constexpr Uint32 Slice = 32;
    static constexpr Uint32 InvalidIndex = std::numeric_limits<Uint32>::max();
    struct FNode {
        DirectX::BoundingBox BoundingBox;
        Uint32 mLeft = InvalidIndex;
        Uint32 mRight = InvalidIndex;
        TObjectRef<UPrimitiveComponent> Component;
    };
    struct FBuildItem {
        DirectX::BoundingBox BoundingBox;
        TObjectRef<UPrimitiveComponent> Component;
    };
    struct MinMaxBox {
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        float maxZ = std::numeric_limits<float>::lowest();

        inline MinMaxBox() = default;

        inline MinMaxBox(const DirectX::BoundingBox& Box)
            : minX(Box.Center.x - Box.Extents.x)
            , minY(Box.Center.y - Box.Extents.y)
            , minZ(Box.Center.z - Box.Extents.z)
            , maxX(Box.Center.x + Box.Extents.x)
            , maxY(Box.Center.y + Box.Extents.y)
            , maxZ(Box.Center.z + Box.Extents.z)
        {
        }

        inline void Expand(const DirectX::BoundingBox& Box) {
            if (minX > Box.Center.x - Box.Extents.x)
                minX = Box.Center.x - Box.Extents.x;
            if (minY > Box.Center.y - Box.Extents.y)
                minY = Box.Center.y - Box.Extents.y;
            if (minZ > Box.Center.z - Box.Extents.z)
                minZ = Box.Center.z - Box.Extents.z;

            if (maxX < Box.Center.x + Box.Extents.x)
                maxX = Box.Center.x + Box.Extents.x;
            if (maxY < Box.Center.y + Box.Extents.y)
                maxY = Box.Center.y + Box.Extents.y;
            if (maxZ < Box.Center.z + Box.Extents.z)
                maxZ = Box.Center.z + Box.Extents.z;
        }

        inline float SurfaceArea() const {
            const float dx = maxX - minX;
            const float dy = maxY - minY;
            const float dz = maxZ - minZ;

            if (dx < 0.0f || dy < 0.0f || dz < 0.0f)
                return 0.0f;

            return 2.0f * (dx * dy + dy * dz + dz * dx);
        }

        static MinMaxBox Merge(const MinMaxBox& Box1, const MinMaxBox& Box2) {
            MinMaxBox Result;
            Result.minX = (std::min)(Box1.minX, Box2.minX);
            Result.minY = (std::min)(Box1.minY, Box2.minY);
            Result.minZ = (std::min)(Box1.minZ, Box2.minZ);

            Result.maxX = (std::max)(Box1.maxX, Box2.maxX);
            Result.maxY = (std::max)(Box1.maxY, Box2.maxY);
            Result.maxZ = (std::max)(Box1.maxZ, Box2.maxZ);
            return Result;
        }
    };
public:
    bool BuildStructure(const TArray<TObjectRef<UPrimitiveComponent>>& Components);
    bool Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld = nullptr) const;
    void WarmupRaycast() const;
    void Clear();
    bool IsBuilt() const { return mBuilt; }
    void InsertComponent(UPrimitiveComponent* Component);
    void UpdateComponent(UPrimitiveComponent* Component);
    void RemoveComponent(UPrimitiveComponent* Component);
private:
    Uint32 MakeChild(TArray<FNode>& BuildNodes, TArray<FBuildItem>& Items, Uint32 First, Uint32 Count, const MinMaxBox& Bounds);
    enum class EProxyKind : Uint8 { Box, Mesh, Billboard };
    struct alignas(64) FProxy {
        FMeshPickingProxy Mesh;
        DirectX::BoundingBox Box;
        TObjectRef<UPrimitiveComponent> Component;
        FVector2 BillboardSize;
        EProxyKind Kind = EProxyKind::Box;
        bool Enabled = false, HasCustomBox = false;
    };
    static_assert(sizeof(FProxy) == 128);
    static Uint64 ComponentKey(FObjectHandle Handle) { return (static_cast<Uint64>(Handle.mGeneration) << 32) | Handle.mIndex; }
    FProxy MakeProxy(UPrimitiveComponent* Component, DirectX::BoundingBox& Bounds);
    bool RaycastProxy(const FProxy& Proxy, const FRay& Ray, float& Distance, const FMatrix* CameraWorld) const;
    std::unordered_map<Uint64, DirectX::BoundingOrientedBox> mLocalBoxes;
    TArray<FProxy> mLeaves;
    TArray<Uint32> mFreeLeaves;
    std::unordered_map<Uint64, Uint32> mLeafIndices;
    FDynamicBVH8 mTree;
    bool mBuilt = false;
};

/// <summary>Provides editor picking over registered primitive component volumes.</summary>
class UPickingSubsystem : public UWorldSubsystem {
public:
    UPickingSubsystem() = default;
    ~UPickingSubsystem() override = default;

    JG_DECLARE_DERIVED_TYPEINFO(UPickingSubsystem, UWorldSubsystem)

    void RegisterComponent(UPrimitiveComponent* Component);
    void UnregisterComponent(UPrimitiveComponent* Component);
    bool RebuildAccelerationStructure();
    void UpdateComponent(UPrimitiveComponent* Component);
    void SynchronizeProxies() const;
    bool Raycast(const FRay& Ray, UPrimitiveComponent*& OutComponent, float& OutDistance, const FMatrix* CameraWorld = nullptr) const;

    bool ContainsComponent(const UPrimitiveComponent* Component) const;
    const TArray<TObjectRef<UPrimitiveComponent>>& GetRegisteredComponents() const;

protected:
    void OnDeinitialize() override;

private:
    TArray<TObjectRef<UPrimitiveComponent>> mComponents{};
    std::unordered_set<Uint64> mRegisteredComponentKeys;
    mutable FWorldRaycastAccelerationStructure RaycastAccelerationStructure{};
    mutable TArray<TObjectRef<UPrimitiveComponent>> mDirtyComponents;
    mutable std::unordered_set<Uint64> mDirtyComponentKeys;
};
