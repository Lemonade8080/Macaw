#include "pch.h"
#include "Corn.h"

namespace BasicGeometry::Cone {
    FGeometry GenerateGeometry() {
        FGeometry Result{};

        Uint32 Vertex{0};
        Uint32 Index{0};

        const float NormalZ{Radius / (HalfHeight * 2.0f)};
        const float NormalLength{std::sqrt(1.0f + NormalZ * NormalZ)};

        for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
            const float U0{static_cast<float>(Segment) / static_cast<float>(Segments)};
            const float U1{static_cast<float>(Segment + 1) / static_cast<float>(Segments)};
            const float Angle0{U0 * std::numbers::pi_v<float> * 2.0f};
            const float Angle1{U1 * std::numbers::pi_v<float> * 2.0f};
            const float MidAngle{(Angle0 + Angle1) * 0.5f};

            const FVector3 Normal0{std::cos(Angle0) / NormalLength, -std::sin(Angle0) / NormalLength, NormalZ / NormalLength};
            const FVector3 Normal1{std::cos(Angle1) / NormalLength, -std::sin(Angle1) / NormalLength, NormalZ / NormalLength};
            const FVector3 ApexNormal{std::cos(MidAngle) / NormalLength, -std::sin(MidAngle) / NormalLength, NormalZ / NormalLength};

            Result.mPositions[Vertex] = FVector3{std::cos(Angle0) * Radius, -std::sin(Angle0) * Radius, -HalfHeight};
            Result.mNormals[Vertex] = Normal0;
            Result.mTexCoords[Vertex++] = FVector2D{U0, 1.0f};

            Result.mPositions[Vertex] = FVector3{0.0f, 0.0f, HalfHeight};
            Result.mNormals[Vertex] = ApexNormal;
            Result.mTexCoords[Vertex++] = FVector2D{(U0 + U1) * 0.5f, 0.0f};

            Result.mPositions[Vertex] = FVector3{std::cos(Angle1) * Radius, -std::sin(Angle1) * Radius, -HalfHeight};
            Result.mNormals[Vertex] = Normal1;
            Result.mTexCoords[Vertex++] = FVector2D{U1, 1.0f};

            Result.mIndices[Index++] = Vertex - 3;
            Result.mIndices[Index++] = Vertex - 2;
            Result.mIndices[Index++] = Vertex - 1;
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

        for (Uint32 Segment{0}; Segment < Segments; ++Segment) {
            Result.mIndices[Index++] = BottomStart;
            Result.mIndices[Index++] = BottomStart + Segment + 1;
            Result.mIndices[Index++] = BottomStart + Segment + 2;
        }

        return Result;
    }
}
