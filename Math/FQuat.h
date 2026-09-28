#pragma once

#include "Math/FVectorRegister.h"
#include "Math/SimpleMath/SimpleMath.h"

using FQuatRegister = FVectorRegister;

//template<typename T>
//struct TVector;
struct FVector;
struct FVector4;
struct FMatrix;
struct FRotator;

struct FQuat
{
    union
    {
        float V[4];
        struct {
            float X;
            float Y;
            float Z;
            float W;
        };
        struct {
            float mX;
            float mY;
            float mZ;
            float mW;
        };
    };

    FQuat();
    FQuat(float InX, float InY, float InZ, float InW = 1.0f);
    explicit FQuat(const FRotator& InRotator);
    FQuat(const FVector& InVector, float InW);
    FQuat(const FVector4& InVector);
    explicit FQuat(const DirectX::XMFLOAT4& Value);
    DirectX::SimpleMath::Quaternion ToSimpleMath() const;

    /* Function */
    FQuat operator * (const FQuat& Other) const;
    FQuat Conjugate() const;
    FQuat UnitInverse() const;
    FQuat Inverse() const;
    void Normalize();
    FVector RotateVector(const FVector& V) const;

    FMatrix ToFMatrix() const;


    static FQuat FromRotator(const FRotator& Rotation);
    static FQuat Concatenate(const FQuat& First, const FQuat& Second);
    FRotator ToRotator() const;

    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;


    /* Static */
    static FQuat Identity();
    static const FQuat CreateFromRotationMatrix(const FMatrix& Matrix);
    const FVector ToEuler() const;
    static const FQuat CreateFromAxisAngle(const FVector Axis, const float AngleRadians);
};