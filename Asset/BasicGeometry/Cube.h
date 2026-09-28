#pragma once

#include <array>
#include "ZUp.h"

namespace BasicGeometry {
    namespace Cube {
        inline const std::array<FVector3, 24> Positions{ToZUp(std::array<FVector3, 24>{
            // Front
            FVector3{-0.5f, -0.5f, 0.5f},
            FVector3{0.5f, -0.5f, 0.5f},
            FVector3{0.5f, 0.5f, 0.5f},
            FVector3{-0.5f, 0.5f, 0.5f},

            // Back
            FVector3{0.5f, -0.5f, -0.5f},
            FVector3{-0.5f, -0.5f, -0.5f},
            FVector3{-0.5f, 0.5f, -0.5f},
            FVector3{0.5f, 0.5f, -0.5f},

            // Left
            FVector3{-0.5f, -0.5f, -0.5f},
            FVector3{-0.5f, -0.5f, 0.5f},
            FVector3{-0.5f, 0.5f, 0.5f},
            FVector3{-0.5f, 0.5f, -0.5f},

            // Right
            FVector3{0.5f, -0.5f, 0.5f},
            FVector3{0.5f, -0.5f, -0.5f},
            FVector3{0.5f, 0.5f, -0.5f},
            FVector3{0.5f, 0.5f, 0.5f},

            // Top
            FVector3{-0.5f, 0.5f, 0.5f},
            FVector3{0.5f, 0.5f, 0.5f},
            FVector3{0.5f, 0.5f, -0.5f},
            FVector3{-0.5f, 0.5f, -0.5f},

            // Bottom
            FVector3{-0.5f, -0.5f, -0.5f},
            FVector3{0.5f, -0.5f, -0.5f},
            FVector3{0.5f, -0.5f, 0.5f},
            FVector3{-0.5f, -0.5f, 0.5f}})};

        inline const std::array<FVector3, 24> Normals{ToZUp(std::array<FVector3, 24>{ FVector3{0.0f, 0.0f, 1.0f}, FVector3{0.0f, 0.0f, 1.0f}, FVector3{0.0f, 0.0f, 1.0f}, FVector3{0.0f, 0.0f, 1.0f}, FVector3{0.0f, 0.0f, -1.0f}, FVector3{0.0f, 0.0f, -1.0f}, FVector3{0.0f, 0.0f, -1.0f}, FVector3{0.0f, 0.0f, -1.0f}, FVector3{-1.0f, 0.0f, 0.0f}, FVector3{-1.0f, 0.0f, 0.0f}, FVector3{-1.0f, 0.0f, 0.0f}, FVector3{-1.0f, 0.0f, 0.0f}, FVector3{1.0f, 0.0f, 0.0f}, FVector3{1.0f, 0.0f, 0.0f}, FVector3{1.0f, 0.0f, 0.0f}, FVector3{1.0f, 0.0f, 0.0f}, FVector3{0.0f, 1.0f, 0.0f}, FVector3{0.0f, 1.0f, 0.0f}, FVector3{0.0f, 1.0f, 0.0f}, FVector3{0.0f, 1.0f, 0.0f}, FVector3{0.0f, -1.0f, 0.0f}, FVector3{0.0f, -1.0f, 0.0f}, FVector3{0.0f, -1.0f, 0.0f}, FVector3{0.0f, -1.0f, 0.0f}})};

        inline const std::array<FVector2D, 24> TexCoords{ FVector2D{0.0f, 1.0f}, FVector2D{1.0f, 1.0f}, FVector2D{1.0f, 0.0f}, FVector2D{0.0f, 0.0f}, FVector2D{0.0f, 1.0f}, FVector2D{1.0f, 1.0f}, FVector2D{1.0f, 0.0f}, FVector2D{0.0f, 0.0f}, FVector2D{0.0f, 1.0f}, FVector2D{1.0f, 1.0f}, FVector2D{1.0f, 0.0f}, FVector2D{0.0f, 0.0f}, FVector2D{0.0f, 1.0f}, FVector2D{1.0f, 1.0f}, FVector2D{1.0f, 0.0f}, FVector2D{0.0f, 0.0f}, FVector2D{0.0f, 1.0f}, FVector2D{1.0f, 1.0f}, FVector2D{1.0f, 0.0f}, FVector2D{0.0f, 0.0f}, FVector2D{0.0f, 1.0f}, FVector2D{1.0f, 1.0f}, FVector2D{1.0f, 0.0f}, FVector2D{0.0f, 0.0f}};

        inline constexpr std::array<Uint32, 36> Indices{ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};
    }
}
