#pragma once

#include <array>
#include <cstddef>

namespace BasicGeometry {
    // Rotate the former Y-up mesh basis +90 degrees around X while preserving winding.
    template <std::size_t Count> constexpr std::array<FVector3, Count> ToZUp(const std::array<FVector3, Count>& Vectors) {
        std::array<FVector3, Count> Result{};

        for (std::size_t Index{0}; Index < Count; ++Index) {
            Result[Index] = FVector3{Vectors[Index].mX, -Vectors[Index].mZ, Vectors[Index].mY};
        }

        return Result;
    }
}
