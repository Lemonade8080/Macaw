#pragma once

struct FVector; 
struct FQuat;

// Degree but Quat: Radians
struct FRotator
{
	union
	{
		struct
		{
			float V[3];
			float Pitch;
			float Yaw;
			float Roll;
		};
	};

	FRotator();
	FRotator(float P, float Y, float R);
	FRotator(FVector Vector);

	static FRotator Identitiy;
	static FRotator Zero;
	
	FQuat Quaternion() const;

	FRotator operator+(const FRotator& Other)
	{
		Pitch += Other.Pitch;
		Yaw += Other.Yaw;
		Roll += Other.Roll;

		return *this;
	}

	
	
};

FRotator operator+(FRotator Rot, const FVector& Vec);



