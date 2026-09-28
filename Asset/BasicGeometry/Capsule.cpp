#include "pch.h"
#include "Capsule.h"

namespace BasicGeometry::Capsule {
    FGeometry GenerateGeometry() {
        FGeometry Result{};

        Uint32 Vertex{0};

        const float Top{HalfCylinderHeight + Radius};
        const float TotalHeight{Top * 2.0f};

        for (Uint32 Ring{0}; Ring <= HemisphereRings; ++Ring) {
            const float T{static_cast<float>(Ring) / static_cast<float>(HemisphereRings)};
            const float Phi{T * std::numbers::pi_v<float> * 0.5f};
            const float RingRadius{std::sin(Phi) * Radius};
            const float Z{HalfCylinderHeight + std::cos(Phi) * Radius};
            const float NormalZ{std::cos(Phi)};
            const float NormalRadius{std::sin(Phi)};
            const float V{(Top - Z) / TotalHeight};

            for (Uint32 Segment{0}; Segment <= Segments; ++Segment) {
                const float U{static_cast<float>(Segment) / static_cast<float>(Segments)};
                const float Theta{U * std::numbers::pi_v<float> * 2.0f};
                const float X{std::cos(Theta)};
                const float Y{-std::sin(Theta)};

                Result.mPositions[Vertex] = FVector3{X * RingRadius, Y * RingRadius, Z};
                Result.mNormals[Vertex] = FVector3{X * NormalRadius, Y * NormalRadius, NormalZ};
                Result.mTexCoords[Vertex++] = FVector2D{U, V};
            }
        }

        for (Uint32 Ring{0}; Ring <= HemisphereRings; ++Ring) {
            const float T{static_cast<float>(Ring) / static_cast<float>(HemisphereRings)};
            const float Phi{T * std::numbers::pi_v<float> * 0.5f};
            const float RingRadius{std::cos(Phi) * Radius};
            const float Z{-HalfCylinderHeight - std::sin(Phi) * Radius};
            const float NormalZ{-std::sin(Phi)};
            const float NormalRadius{std::cos(Phi)};
            const float V{(Top - Z) / TotalHeight};

            for (Uint32 Segment{0}; Segment <= Segments; ++Segment) {
                const float U{static_cast<float>(Segment) / static_cast<float>(Segments)};
                const float Theta{U * std::numbers::pi_v<float> * 2.0f};
                const float X{std::cos(Theta)};
                const float Y{-std::sin(Theta)};

                Result.mPositions[Vertex] = FVector3{X * RingRadius, Y * RingRadius, Z};
                Result.mNormals[Vertex] = FVector3{X * NormalRadius, Y * NormalRadius, NormalZ};
                Result.mTexCoords[Vertex++] = FVector2D{U, V};
            }
        }

        Uint32 Index{0};

        for (Uint32 Ring{0}; Ring < RingCount - 1; ++Ring) {
            for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
                const Uint32 TopLeft{Ring * (Segments + 1) + Segment};
                const Uint32 TopRight{TopLeft + 1};
                const Uint32 BottomLeft{(Ring + 1) * (Segments + 1) + Segment};
                const Uint32 BottomRight{BottomLeft + 1};

                Result.mIndices[Index++] = TopLeft;
                Result.mIndices[Index++] = TopRight;
                Result.mIndices[Index++] = BottomRight;

                Result.mIndices[Index++] = TopLeft;
                Result.mIndices[Index++] = BottomRight;
                Result.mIndices[Index++] = BottomLeft;
            }
        }

        return Result;
    }
}
