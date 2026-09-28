#include "pch.h"
#include "Math/FRotator.h"
#include "Math/FMath.h"

FRotator::FRotator()
{
	Pitch = 0; Yaw = 0; Roll = 0;
}

FRotator::FRotator(float P, float Y, float R)
{
	Pitch = P; Yaw = Y; Roll = R;
}

FRotator::FRotator(FVector Vector)
{
	Pitch = Vector.X; Yaw = Vector.Y; Roll = Vector.Z;
}

FQuat FRotator::Quaternion() const
{
	return FQuat::FromRotator(*this);
}

FRotator FRotator::Identitiy = FRotator(0, 0, 0);
FRotator FRotator::Zero = FRotator(0.f, 0.f, 0.f);

FRotator operator+(FRotator Rot, const FVector& Vec)
{
	Rot.Roll += Vec.X;
	Rot.Pitch += Vec.Y;
	Rot.Yaw += Vec.Z;

	return Rot;
}