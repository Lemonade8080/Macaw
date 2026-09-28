#pragma once

#include <array>
#include <numbers>
#include <cmath>

namespace BasicGeometry {
    namespace Sphere {
        constexpr Uint32 Segments{32};
        constexpr Uint32 Rings{16};
        constexpr float Radius{0.5f};

        constexpr Uint32 VertexCount{(Rings + 1) * (Segments + 1)};
        constexpr Uint32 IndexCount{Rings * Segments * 6};

        struct FSphereGeometry {
            std::array<FVector3, VertexCount> mPositions{};
            std::array<FVector3, VertexCount> mNormals{};
            std::array<FVector2D, VertexCount> mTexCoords{};
            std::array<Uint32, IndexCount> mIndices{};
        };

        FSphereGeometry GenerateSphere();

        inline const FSphereGeometry Geometry{GenerateSphere()};

        inline const auto& Positions{Geometry.mPositions};
        inline const auto& Normals{Geometry.mNormals};
        inline const auto& TexCoords{Geometry.mTexCoords};
        inline const auto& Indices{Geometry.mIndices};
    }
}
