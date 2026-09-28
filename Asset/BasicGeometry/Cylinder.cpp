#include "pch.h"
#include "Cylinder.h"

namespace BasicGeometry::Cylinder {
    FGeometry GenerateGeometry() {
        FGeometry Result{};

        Uint32 Vertex{0};

        for (Uint32 Segment{0}; Segment <= Segments; ++Segment) {
            const float U{static_cast<float>(Segment) / static_cast<float>(Segments)};
            const float Angle{U * std::numbers::pi_v<float> * 2.0f};
            const float X{std::cos(Angle)};
            const float Y{-std::sin(Angle)};

            Result.mPositions[Vertex] = FVector3{X * Radius, Y * Radius, -HalfHeight};
            Result.mNormals[Vertex] = FVector3{X, Y, 0.0f};
            Result.mTexCoords[Vertex++] = FVector2D{U, 1.0f};

            Result.mPositions[Vertex] = FVector3{X * Radius, Y * Radius, HalfHeight};
            Result.mNormals[Vertex] = FVector3{X, Y, 0.0f};
            Result.mTexCoords[Vertex++] = FVector2D{U, 0.0f};
        }

        const Uint32 TopStart{Vertex};

        Result.mPositions[Vertex] = FVector3{0.0f, 0.0f, HalfHeight};
        Result.mNormals[Vertex] = FVector3{0.0f, 0.0f, 1.0f};
        Result.mTexCoords[Vertex++] = FVector2D{0.5f, 0.5f};

        for (Uint32 Segment{0}; Segment <= Segments; ++Segment) {
            const float Angle{static_cast<float>(Segment) / static_cast<float>(Segments) * std::numbers::pi_v<float> * 2.0f};
            const float X{std::cos(Angle)};
            const float Y{-std::sin(Angle)};

            Result.mPositions[Vertex] = FVector3{X * Radius, Y * Radius, HalfHeight};
            Result.mNormals[Vertex] = FVector3{0.0f, 0.0f, 1.0f};
            Result.mTexCoords[Vertex++] = FVector2D{X * 0.5f + 0.5f, -Y * 0.5f + 0.5f};
        }

        const Uint32 BottomStart{Vertex};

        Result.mPositions[Vertex] = FVector3{0.0f, 0.0f, -HalfHeight};
        Result.mNormals[Vertex] = FVector3{0.0f, 0.0f, -1.0f};
        Result.mTexCoords[Vertex++] = FVector2D{0.5f, 0.5f};

        for (Uint32 Segment{0}; Segment <= Segments; ++Segment) {
            const float Angle{static_cast<float>(Segment) / static_cast<float>(Segments) * std::numbers::pi_v<float> * 2.0f};
            const float X{std::cos(Angle)};
            const float Y{-std::sin(Angle)};

            Result.mPositions[Vertex] = FVector3{X * Radius, Y * Radius, -HalfHeight};
            Result.mNormals[Vertex] = FVector3{0.0f, 0.0f, -1.0f};
            Result.mTexCoords[Vertex++] = FVector2D{X * 0.5f + 0.5f, Y * 0.5f + 0.5f};
        }

        Uint32 Index{0};

        for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
            const Uint32 Bottom0{Segment * 2};
            const Uint32 Top0{Bottom0 + 1};
            const Uint32 Bottom1{Bottom0 + 2};
            const Uint32 Top1{Bottom0 + 3};

            Result.mIndices[Index++] = Bottom0;
            Result.mIndices[Index++] = Top0;
            Result.mIndices[Index++] = Top1;

            Result.mIndices[Index++] = Bottom0;
            Result.mIndices[Index++] = Top1;
            Result.mIndices[Index++] = Bottom1;
        }

        for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
            Result.mIndices[Index++] = TopStart;
            Result.mIndices[Index++] = TopStart + Segment + 2;
            Result.mIndices[Index++] = TopStart + Segment + 1;
        }

        for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
            Result.mIndices[Index++] = BottomStart;
            Result.mIndices[Index++] = BottomStart + Segment + 1;
            Result.mIndices[Index++] = BottomStart + Segment + 2;
        }

        return Result;
    }
}
