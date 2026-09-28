#include "pch.h"
#include "InverseSphere.h"

namespace BasicGeometry::SkyDome {
    FGeometry GenerateGeometry() {
        FGeometry Result{};

        Uint32 Vertex{0};

        for (Uint32 Latitude{0}; Latitude <= LatitudeSegments; ++Latitude) {
            const float V{static_cast<float>(Latitude) / static_cast<float>(LatitudeSegments)};
            const float Phi{V * std::numbers::pi_v<float>};

            const float Z{std::cos(Phi)};
            const float RingRadius{std::sin(Phi)};

            for (Uint32 Longitude{0}; Longitude <= LongitudeSegments; ++Longitude) {
                const float U{static_cast<float>(Longitude) / static_cast<float>(LongitudeSegments)};
                const float Theta{U * std::numbers::pi_v<float> * 2.0f};

                const float X{std::cos(Theta) * RingRadius};
                const float Y{-std::sin(Theta) * RingRadius};

                Result.mPositions[Vertex] = FVector3{ X * Radius, Y * Radius, Z * Radius};

                Result.mNormals[Vertex] = FVector3{ -X, -Y, -Z};

                Result.mTexCoords[Vertex++] = FVector2D{ U, V};
            }
        }

        Uint32 Index{0};

        for (Uint32 Latitude{0}; Latitude < LatitudeSegments; ++Latitude) {
            for (Uint32 Longitude{0}; Longitude < LongitudeSegments; ++Longitude) {
                const Uint32 TopLeft{Latitude * (LongitudeSegments + 1) + Longitude};
                const Uint32 TopRight{TopLeft + 1};
                const Uint32 BottomLeft{(Latitude + 1) * (LongitudeSegments + 1) + Longitude};
                const Uint32 BottomRight{BottomLeft + 1};

                Result.mIndices[Index++] = TopLeft;
                Result.mIndices[Index++] = BottomRight;
                Result.mIndices[Index++] = TopRight;

                Result.mIndices[Index++] = TopLeft;
                Result.mIndices[Index++] = BottomLeft;
                Result.mIndices[Index++] = BottomRight;
            }
        }

        return Result;
    }
}
