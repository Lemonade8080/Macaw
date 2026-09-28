#pragma once

#include <array>
#include <cmath>
#include <numbers>

namespace BasicGeometry {
    namespace Cylinder {
        inline constexpr Uint32 Segments{32};
        inline constexpr float Radius{0.5f};
        inline constexpr float HalfHeight{0.5f};

        inline constexpr Uint32 SideVertexCount{(Segments + 1) * 2};
        inline constexpr Uint32 CapVertexCount{Segments + 2};
        inline constexpr Uint32 VertexCount{SideVertexCount + CapVertexCount * 2};
        inline constexpr Uint32 IndexCount{Segments * 12};

        struct FGeometry {
            std::array<FVector3, VertexCount> mPositions{};
            std::array<FVector3, VertexCount> mNormals{};
            std::array<FVector2D, VertexCount> mTexCoords{};
            std::array<Uint32, IndexCount> mIndices{};
        };

        FGeometry GenerateGeometry();

        inline const FGeometry Geometry{GenerateGeometry()};

        inline const auto& Positions{Geometry.mPositions};
        inline const auto& Normals{Geometry.mNormals};
        inline const auto& TexCoords{Geometry.mTexCoords};
        inline const auto& Indices{Geometry.mIndices};
    }
}
