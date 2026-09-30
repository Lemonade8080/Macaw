#pragma once

#include <memory>
#include "Core/Base/FTransform.h"

class UMesh;
struct FMeshPickingSource;

struct FMeshPickingProxy {
    std::shared_ptr<const FMeshPickingSource> Source;
    DirectX::XMFLOAT3 InverseScale{}, Position{};
    DirectX::XMFLOAT4 Rotation{};
    bool Valid = false, ReverseWinding = false;

    const UMesh* GetMesh() const;
    void Update(const UMesh* InMesh, const FTransform& Transform);
    bool Raycast(const FRay& Ray, float& OutDistance, float MaxDistance) const;
};
