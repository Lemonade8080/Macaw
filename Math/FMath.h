#pragma once

#include <cmath>
#include "Math/FVector.h"
#include "Math/FVector2.h"
#include "Math/FVector4.h"
#include "Math/FMatrix.h"
#include "Math/FRotator.h"
#include "Math/FQuat.h"

#include "Math/FVectorRegister.h"
#include "Math/FMatrixRegister.h"

#define PI 3.141592f

using FPlane = DirectX::SimpleMath::Plane;
using FRay = DirectX::SimpleMath::Ray;

namespace FMath
{
    //static float PI = std::acosf(-1);

    static inline bool IsNearlyZero(float Value, float ErrorTolerance = 1e-8f)
    {
        return std::abs(Value) <= ErrorTolerance;
    }

    static inline bool IsNearlyEqual(float Value1, float Value2, float ErrorTolerance = 1e-4f)
    {
        return std::abs(Value1 - Value2) <= ErrorTolerance;
    }

    static inline float Clamp(float Value, float Min, float Max)
    {
        if (Value < Min) return Min;
        if (Value > Max) return Max;
        return Value;
    }

    static inline float RadiansToDegrees(float Radian)
    {
        return Radian * (180.0f / PI);
    }

    static inline float DegreesToRadians(float Degree)
    {
        return Degree * (PI / 180.0f);
    }
}