#include "pch.h"
#include "Sphere.h"

namespace BasicGeometry::Sphere {
    FSphereGeometry GenerateSphere() {
        FSphereGeometry Geometry{};

        Uint32 VertexIndex{0};

        for (Uint32 Ring{0}; Ring <= Rings; ++Ring) {
            const float V{static_cast<float>(Ring) / Rings};
            const float Phi{V * std::numbers::pi_v<float>};

            const float Z{std::cos(Phi)};
            const float RingRadius{std::sin(Phi)};

            for (Uint32 Segment{0}; Segment <= Segments; ++Segment) {
                const float U{static_cast<float>(Segment) / Segments};
                const float Theta{U * std::numbers::pi_v<float> * 2.0f};

                const float X{RingRadius * std::cos(Theta)};
                const float Y{-RingRadius * std::sin(Theta)};

                Geometry.mPositions[VertexIndex] = FVector3{X * Radius, Y * Radius, Z * Radius};
                Geometry.mNormals[VertexIndex] = FVector3{X, Y, Z};
                Geometry.mTexCoords[VertexIndex] = FVector2D{U, V};

                ++VertexIndex;
            }
        }

        Uint32 Index{0};

        for (Uint32 Ring{0}; Ring < Rings; ++Ring) {
            for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
                const Uint32 TopLeft{Ring * (Segments + 1) + Segment};
                const Uint32 TopRight{TopLeft + 1};
                const Uint32 BottomLeft{(Ring + 1) * (Segments + 1) + Segment};
                const Uint32 BottomRight{BottomLeft + 1};

                Geometry.mIndices[Index++] = TopLeft;
                Geometry.mIndices[Index++] = TopRight;
                Geometry.mIndices[Index++] = BottomLeft;

                Geometry.mIndices[Index++] = TopRight;
                Geometry.mIndices[Index++] = BottomRight;
                Geometry.mIndices[Index++] = BottomLeft;
            }
        }

        return Geometry;
    }
}
