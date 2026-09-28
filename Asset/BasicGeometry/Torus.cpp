#include "pch.h"
#include "Torus.h"

namespace BasicGeometry::Torus {
    FGeometry GenerateGeometry(float InMajorRadius, float InMinorRadius) {
        FGeometry Result{};

        Uint32 Vertex{0};

        for (Uint32 Major{0}; Major <= MajorSegments; ++Major) {
            const float U{static_cast<float>(Major) / static_cast<float>(MajorSegments)};
            const float Theta{U * std::numbers::pi_v<float> * 2.0f};
            const float CosTheta{std::cos(Theta)};
            const float SinTheta{std::sin(Theta)};

            for (Uint32 Minor{0}; Minor <= MinorSegments; ++Minor) {
                const float V{static_cast<float>(Minor) / static_cast<float>(MinorSegments)};
                const float Phi{V * std::numbers::pi_v<float> * 2.0f};
                const float CosPhi{std::cos(Phi)};
                const float SinPhi{std::sin(Phi)};

                const float RingRadius{InMajorRadius + InMinorRadius * CosPhi};

                Result.mPositions[Vertex] = FVector3{RingRadius * CosTheta, -RingRadius * SinTheta, InMinorRadius * SinPhi};
                Result.mNormals[Vertex] = FVector3{CosPhi * CosTheta, -CosPhi * SinTheta, SinPhi};
                Result.mTexCoords[Vertex++] = FVector2D{U, V};
            }
        }

        Uint32 Index{0};

        for (Uint32 Major{0}; Major < MajorSegments; ++Major) {
            for (Uint32 Minor{0}; Minor < MinorSegments; ++Minor) {
                const Uint32 Current{Major * (MinorSegments + 1) + Minor};
                const Uint32 MinorNext{Current + 1};
                const Uint32 MajorNext{(Major + 1) * (MinorSegments + 1) + Minor};
                const Uint32 MajorMinorNext{MajorNext + 1};

                Result.mIndices[Index++] = Current;
                Result.mIndices[Index++] = MinorNext;
                Result.mIndices[Index++] = MajorMinorNext;

                Result.mIndices[Index++] = Current;
                Result.mIndices[Index++] = MajorMinorNext;
                Result.mIndices[Index++] = MajorNext;
            }
        }

        return Result;
    }
}
