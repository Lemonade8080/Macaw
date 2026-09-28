#include "pch.h"
#include "Math/FQuat.h"
#include "Math/FRotator.h"
#include "Math/FMath.h"
#include "Math/FVector4.h"
#include <cfloat>

FQuat::FQuat()
{
	X = 0.0f; Y = 0.0f; Z = 0.0f; W = 1.0f;
}

FQuat::FQuat(float InX, float InY, float InZ, float InW)
{
	X = InX; Y = InY; Z = InZ; W = InW;
}

FQuat::FQuat(const FRotator& InRotator)
{
	*this = FromRotator(InRotator);
}

FQuat::FQuat(const FVector& InVector, float InW)
	: X(InVector.X), Y(InVector.Y), Z(InVector.Z), W(InW)
{
}

FQuat::FQuat(const FVector4& InVector)
	: X(InVector.X), Y(InVector.Y), Z(InVector.Z), W(InVector.W)
{
}

FQuat::FQuat(const DirectX::XMFLOAT4& Value)
	: X(Value.x), Y(Value.y), Z(Value.z), W(Value.w)
{
}

DirectX::SimpleMath::Quaternion FQuat::ToSimpleMath() const
{
	return {X, Y, Z, W};
}

/* Functions */

FQuat FQuat::operator * (const FQuat& Other) const
{
	FQuatRegister A = VectorSIMD::Load(V);
	FQuatRegister B = VectorSIMD::Load(Other.V);

	// i 성분: A0 * B3 + A3 * B0 + A1 * B2 - A2 * B1
	FQuatRegister IA = VectorSIMD::Shuffle<0, 3, 1, 2>(A, A);
	FQuatRegister IB = VectorSIMD::Shuffle<3, 0, 2, 1>(B, B);

	// j 성분: A1 * B3 + A3 * B1 + A2 * B0 - A0 * B2
	FQuatRegister JA = VectorSIMD::Shuffle<1, 3, 2, 0>(A, A);
	FQuatRegister JB = VectorSIMD::Shuffle<3, 1, 0, 2>(B, B);

	// k 성분: A3 * B2 + A2 * B3 + A0 * B1 - A1 * B0
	FQuatRegister KA = VectorSIMD::Shuffle<3, 2, 0, 1>(A, A);
	FQuatRegister KB = VectorSIMD::Shuffle<2, 3, 1, 0>(B, B);

	// R 성분: -( A0 * B0 + A1 * B1 + A2 * B2 - A3 * B3 )

	FQuatRegister Sign = VectorSIMD::SetVal(1.0f, 1.0f, 1.0f, -1.0f);

	FQuatRegister I = VectorSIMD::Mul(IA, IB);
	FQuatRegister J = VectorSIMD::Mul(JA, JB);
	FQuatRegister K = VectorSIMD::Mul(KA, KB);
	FQuatRegister R = VectorSIMD::Mul(A, B);

	float SI = VectorSIMD::Dot(I, Sign);
	float SJ = VectorSIMD::Dot(J, Sign);
	float SK = VectorSIMD::Dot(K, Sign);
	float SR = -VectorSIMD::Dot(R, Sign);

	return FQuat(SI, SJ, SK, SR);

}

// 켤레 복소수
FQuat FQuat::Conjugate() const
{
	FQuatRegister Reg = VectorSIMD::Load(V);
	FQuatRegister Sign = VectorSIMD::SetVal(-1.0f, -1.0f, -1.0f, 1.0f);
	FQuat Result;
	VectorSIMD::Store(Result.V, VectorSIMD::Mul(Reg, Sign));
	return Result;
}

// 단위 쿼터니언 전용 역
FQuat FQuat::UnitInverse() const
{
	return Conjugate();
}

FQuat FQuat::Inverse() const
{
	FQuatRegister Reg = VectorSIMD::Load(V);
	FQuatRegister Sign = VectorSIMD::SetVal(-1.0f, -1.0f, -1.0f, 1.0f);

	// Conjugate
	FQuatRegister Conjugate = VectorSIMD::Mul(Reg, Sign);
	FQuatRegister LengthSq = VectorSIMD::SetVal(VectorSIMD::LengthSquared(Reg));
	FQuatRegister ResultReg = VectorSIMD::Div(Conjugate, LengthSq);

	FQuat Result;
	VectorSIMD::Store(Result.V, ResultReg);

	return Result;
}

void FQuat::Normalize()
{
	const float LengthSquared = X * X + Y * Y + Z * Z + W * W;
	if (LengthSquared <= 1e-8f)
	{
		X = Y = Z = 0.0f;
		W = 1.0f;
		return;
	}
	const float InverseLength = 1.0f / std::sqrt(LengthSquared);
	X *= InverseLength;
	Y *= InverseLength;
	Z *= InverseLength;
	W *= InverseLength;
}

FVector FQuat::RotateVector(const FVector& V) const
{
	FQuatRegister Q = VectorSIMD::Load(this->V);
	FVectorRegister Vec = VectorSIMD::LoadFloat3(V.V);

	// Q.xyz
	FVectorRegister Qv = VectorSIMD::SetWZero(Q);

	// T = 2 * cross(Q.xyz, V)
	FVectorRegister T = VectorSIMD::Mul(VectorSIMD::SetVal(2.0f), VectorSIMD::Cross3(Qv, Vec));

	// V + W*T
	FVectorRegister Result = VectorSIMD::Add(Vec, VectorSIMD::Mul(VectorSIMD::SplatW(Q), T));

	// + cross(Q.xyz, T)
	Result = VectorSIMD::Add(Result, VectorSIMD::Cross3(Qv, T));

	FVector Out;
	VectorSIMD::StoreFloat3(Out.V, Result);

	return Out;
}

FQuat FQuat::Concatenate(const FQuat& First, const FQuat& Second)
{
	return First * Second;
}

FMatrix FQuat::ToFMatrix() const
{
	const float XX = X * X;
	const float YY = Y * Y;
	const float ZZ = Z * Z;

	const float XY = X * Y;
	const float XZ = X * Z;
	const float YZ = Y * Z;

	const float WX = W * X;
	const float WY = W * Y;
	const float WZ = W * Z;

	return FMatrix(
		1.0f - 2.0f * (YY + ZZ), 2.0f * (XY + WZ), 2.0f * (XZ - WY), 0.0f,
		2.0f * (XY - WZ), 1.0f - 2.0f * (XX + ZZ), 2.0f * (YZ + WX), 0.0f,
		2.0f * (XZ + WY), 2.0f * (YZ - WX), 1.0f - 2.0f * (XX + YY), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

FRotator FQuat::ToRotator() const
{
	FRotator ResultRotator;

	// Pitch
	const float SinPitch = 2.0f * (W * Y - Z * X);

	// Gimbal Lock 방지를 위해 [-1, 1] 범위로 Clamp
	const float ClampedSinPitch = FMath::Clamp(SinPitch, -1.0f, 1.0f);

	ResultRotator.Pitch = asinf(ClampedSinPitch);

	// Roll
	ResultRotator.Roll = atan2f(2.0f * (W * X + Y * Z), 1.0f - 2.0f * (X * X + Y * Y));

	// Yaw
	ResultRotator.Yaw = atan2f(2.0f * (W * Z + X * Y), 1.0f - 2.0f * (Y * Y + Z * Z));

	// Radian -> Degree
	ResultRotator.Pitch = FMath::RadiansToDegrees(ResultRotator.Pitch);
	ResultRotator.Yaw = FMath::RadiansToDegrees(ResultRotator.Yaw);
	ResultRotator.Roll = FMath::RadiansToDegrees(ResultRotator.Roll);

	return ResultRotator;
}

FQuat FQuat::FromRotator(const FRotator& Rotation)
{
	const float PitchRadians = FMath::DegreesToRadians(Rotation.Pitch);
	const float YawRadians = FMath::DegreesToRadians(Rotation.Yaw);
	const float RollRadians = FMath::DegreesToRadians(Rotation.Roll);

	const FQuat Pitch = CreateFromAxisAngle(FVector::UnitX, PitchRadians);
	const FQuat Yaw = CreateFromAxisAngle(FVector::UnitZ, YawRadians);
	const FQuat Roll = CreateFromAxisAngle(FVector::UnitY, RollRadians);

	FQuat Result = (Roll * Pitch) * Yaw;
	Result.Normalize();
	return Result;
}

const FQuat FQuat::CreateFromRotationMatrix(const FMatrix& Matrix)
{
	FQuat Result;
	const float R22 = Matrix.M[2][2];
	if (R22 <= 0.0f)
	{
		const float Dif10 = Matrix.M[1][1] - Matrix.M[0][0];
		const float Omr22 = 1.0f - R22;
		if (Dif10 <= 0.0f)
		{
			const float FourXSqr = Omr22 - Dif10;
			const float Inv4X = 0.5f / std::sqrt(FourXSqr);
			Result.X = FourXSqr * Inv4X;
			Result.Y = (Matrix.M[0][1] + Matrix.M[1][0]) * Inv4X;
			Result.Z = (Matrix.M[0][2] + Matrix.M[2][0]) * Inv4X;
			Result.W = (Matrix.M[1][2] - Matrix.M[2][1]) * Inv4X;
		}
		else
		{
			const float FourYSqr = Omr22 + Dif10;
			const float Inv4Y = 0.5f / std::sqrt(FourYSqr);
			Result.X = (Matrix.M[0][1] + Matrix.M[1][0]) * Inv4Y;
			Result.Y = FourYSqr * Inv4Y;
			Result.Z = (Matrix.M[1][2] + Matrix.M[2][1]) * Inv4Y;
			Result.W = (Matrix.M[2][0] - Matrix.M[0][2]) * Inv4Y;
		}
	}
	else
	{
		const float Sum10 = Matrix.M[1][1] + Matrix.M[0][0];
		const float Opr22 = 1.0f + R22;
		if (Sum10 <= 0.0f)
		{
			const float FourZSqr = Opr22 - Sum10;
			const float Inv4Z = 0.5f / std::sqrt(FourZSqr);
			Result.X = (Matrix.M[0][2] + Matrix.M[2][0]) * Inv4Z;
			Result.Y = (Matrix.M[1][2] + Matrix.M[2][1]) * Inv4Z;
			Result.Z = FourZSqr * Inv4Z;
			Result.W = (Matrix.M[0][1] - Matrix.M[1][0]) * Inv4Z;
		}
		else
		{
			const float FourWSqr = Opr22 + Sum10;
			const float Inv4W = 0.5f / std::sqrt(FourWSqr);
			Result.X = (Matrix.M[1][2] - Matrix.M[2][1]) * Inv4W;
			Result.Y = (Matrix.M[2][0] - Matrix.M[0][2]) * Inv4W;
			Result.Z = (Matrix.M[0][1] - Matrix.M[1][0]) * Inv4W;
			Result.W = FourWSqr * Inv4W;
		}
	}
	return Result;
}

const FVector FQuat::ToEuler() const
{
	const float XX = X * X;
	const float YY = Y * Y;
	const float ZZ = Z * Z;
	const float M31 = 2.0f * X * Z + 2.0f * Y * W;
	const float M32 = 2.0f * Y * Z - 2.0f * X * W;
	const float M33 = 1.0f - 2.0f * XX - 2.0f * YY;
	const float CY = std::sqrt(M33 * M33 + M31 * M31);
	const float CX = std::atan2(-M32, CY);
	if (CY > 16.0f * FLT_EPSILON)
	{
		const float M12 = 2.0f * X * Y + 2.0f * Z * W;
		const float M22 = 1.0f - 2.0f * XX - 2.0f * ZZ;
		return FVector(CX, std::atan2(M31, M33), std::atan2(M12, M22));
	}
	const float M11 = 1.0f - 2.0f * YY - 2.0f * ZZ;
	const float M21 = 2.0f * X * Y - 2.0f * Z * W;
	return FVector(CX, 0.0f, std::atan2(-M21, M11));
}

FVector FQuat::GetForwardVector() const
{
	return FVector(
		1.0f - 2.0f * (Y * Y + Z * Z),
		2.0f * (X * Y + W * Z),
		2.0f * (X * Z - W * Y)
	);
}

FVector FQuat::GetRightVector() const
{
	return FVector(
		2.0f * (X * Y - W * Z),
		1.0f - 2.0f * (X * X + Z * Z),
		2.0f * (Y * Z + W * X)
	);
}

FVector FQuat::GetUpVector() const
{
	return FVector(
		2.0f * (X * Z + W * Y),
		2.0f * (Y * Z - W * X),
		1.0f - 2.0f * (X * X + Y * Y)
	);
}

/* Statics */

FQuat FQuat::Identity()
{
	return FQuat(0, 0, 0, 1.0f);
}

const FQuat FQuat::CreateFromAxisAngle(const FVector Axis, const float AngleRadians)
{
	FVector NormalizedAxis = Axis;
	NormalizedAxis.Normalize();

	const float HalfAngle = AngleRadians * 0.5f;

	const float S = sinf(HalfAngle);
	const float C = cosf(HalfAngle);

	return FQuat(
		NormalizedAxis.X * S,
		NormalizedAxis.Y * S,
		NormalizedAxis.Z * S,
		C
	);
}

