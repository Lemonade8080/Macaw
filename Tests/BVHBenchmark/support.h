#pragma once
#include "pch.h"
#include <DirectXCollision.h>
#include <ranges>
#include <span>
#include <fstream>
#include <random>
#include <chrono>
#include <cstdio>
#include <stdexcept>
enum class EVertexAttribute { Position };
using Position = FVector;
class UMesh {
public:
    std::vector<Position> Positions;
    TArray<Uint32> Indices;
    template<EVertexAttribute> std::span<const Position> GetVertexAttributeData() const { return Positions; }
    const TArray<Uint32>& GetIndices() const { return Indices; }
};
