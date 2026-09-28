#include "Math/FMatrix.h"
#include "Math/FMath.h"
#include "Math/FVector.h"
#include "Math/FVector4.h"

FMatrix::FMatrix()
{
	SetIdentity();
}

FMatrix::FMatrix(const FVector4& InX, const FVector4& InY, const FVector4& InZ, const FVector4& InW)
{
	M[0][0] = InX.X; M[0][1] = InX.Y; M[0][2] = InX.Z; M[0][3] = InX.W;
	M[1][0] = InY.X; M[1][1] = InY.Y; M[1][2] = InY.Z; M[1][3] = InY.W;
	M[2][0] = InZ.X; M[2][1] = InZ.Y; M[2][2] = InZ.Z; M[2][3] = InZ.W;
	M[3][0] = InW.X; M[3][1] = InW.Y; M[3][2] = InW.Z; M[3][3] = InW.W;
}

FMatrix::FMatrix(
	const float& f00, const float& f01, const float& f02, const float& f03,
	const float& f10, const float& f11, const float& f12, const float& f13,
	const float& f20, const float& f21, const float& f22, const float& f23,
	const float& f30, const float& f31, const float& f32, const float& f33
) 
{
	M[0][0] = f00; M[0][1] = f01; M[0][2] = f02; M[0][3] = f03;
	M[1][0] = f10; M[1][1] = f11; M[1][2] = f12; M[1][3] = f13;
	M[2][0] = f20; M[2][1] = f21; M[2][2] = f22; M[2][3] = f23;
	M[3][0] = f30; M[3][1] = f31; M[3][2] = f32; M[3][3] = f33;
}

/* Axis */

void FMatrix::SetAxes(const FVector4& Axis0, const FVector4& Axis1, const FVector4& Axis2, const FVector4& Axis3)
{
	// 수정 필요
	M[0][0] = Axis0.X; M[0][1] = Axis0.Y; M[0][2] = Axis0.Z; M[0][3] = Axis0.W;
	M[1][0] = Axis1.X; M[1][1] = Axis1.Y; M[1][2] = Axis1.Z; M[1][3] = Axis1.W;
	M[2][0] = Axis2.X; M[2][1] = Axis2.Y; M[2][2] = Axis2.Z; M[2][3] = Axis2.W;
	M[3][0] = Axis3.X; M[3][1] = Axis3.Y; M[3][2] = Axis3.Z; M[3][3] = Axis3.W;
}

void FMatrix::SetAxis(int32 i, const FVector& Axis)
{
	M[i][0] = Axis.X;
	M[i][1] = Axis.Y;
	M[i][2] = Axis.Z;
}

void FMatrix::GetUnitAxis(FVector4& X, FVector4& Y, FVector4& Z) const
{
	X = FVector4(M[0][0], M[0][1], M[0][2], 0.0f);
	Y = FVector4(M[1][0], M[1][1], M[1][2], 0.0f);
	Z = FVector4(M[2][0], M[2][1], M[2][2], 0.0f);
	X.Normalize();
	Y.Normalize();
	Z.Normalize();
}


FVector4 FMatrix::GetColumn(int32 i) const
{
	return FVector4(M[0][i], M[1][i], M[2][i], M[3][i]);
}

FVector4 FMatrix::GetOrigin()
{
	return FVector4(M[3][0], M[3][1], M[3][2], M[3][3]);
}

//FVector4 FMatrix::GetScaledAxis(FVector4& X, FVector4& Y, FVector4& Z) const
//{
//	return FVector4();
//}


FVector4 FMatrix::TransformFVector4(const FVector4& V) const
{
	FMatrixRegister MReg = FMatrixRegister::Load(*this);
	MReg = MReg.Transpose();

	FVectorRegister VReg = VectorSIMD::SetVal(V.X, V.Y, V.Z, V.W);

	return FVector4(
		VectorSIMD::Dot(MReg.R[0], VReg),
		VectorSIMD::Dot(MReg.R[1], VReg),
		VectorSIMD::Dot(MReg.R[2], VReg),
		VectorSIMD::Dot(MReg.R[3], VReg)
	);
}

FVector FMatrix::TransformPosition(const FVector& V) const
{
	return FVector(
		V.X * M[0][0] + V.Y * M[1][0] + V.Z * M[2][0] + M[3][0],
		V.X * M[0][1] + V.Y * M[1][1] + V.Z * M[2][1] + M[3][1],
		V.X * M[0][2] + V.Y * M[1][2] + V.Z * M[2][2] + M[3][2]);
}

FVector4 FMatrix::TransformVector(const FVector& V) const
{
	return TransformFVector4(FVector4(V.X, V.Y, V.Z, 0.0f));
}

FVector4 FMatrix::InverseTransformPosition(const FVector& V) const
{
	FMatrix InvMatrix = Inverse();
	return InvMatrix.TransformFVector4(FVector4(V, 1.0f));
}


/* Functions */

FMatrix FMatrix::ApplyScale(float Scale) const
{
	return FMatrix(
		FVector4(M[0][0] * Scale, M[0][1], M[0][2], M[0][3]),
		FVector4(M[1][0], M[1][1] * Scale, M[1][2], M[1][3]),
		FVector4(M[2][0], M[2][1], M[2][2] * Scale, M[2][3]),
		FVector4(M[3][0], M[3][1], M[3][2], M[3][3] * Scale)
	);
}

FMatrix FMatrix::GetTransposed() const
{
	return FMatrixRegister::Load(*this).Transpose().ToFMatrix();
}

FMatrix FMatrix::Inverse() const
{
	return FMatrixRegister::Load(*this).Inverse().ToFMatrix();
}

FMatrix FMatrix::Multiply(const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	FMatrixRegister Result;

	for (int32 i = 0; i < 4; ++i)
	{
		FVectorRegister X = VectorSIMD::Mul(VectorSIMD::Swizzle<0, 0, 0, 0>(A.R[i]), B.R[0]);
		FVectorRegister Y = VectorSIMD::Mul(VectorSIMD::Swizzle<1, 1, 1, 1>(A.R[i]), B.R[1]);
		FVectorRegister Z = VectorSIMD::Mul(VectorSIMD::Swizzle<2, 2, 2, 2>(A.R[i]), B.R[2]);
		FVectorRegister W = VectorSIMD::Mul(VectorSIMD::Swizzle<3, 3, 3, 3>(A.R[i]), B.R[3]);

		Result.R[i] = VectorSIMD::Add(VectorSIMD::Add(X, Y), VectorSIMD::Add(Z, W));
	}

	return Result.ToFMatrix();
}


float FMatrix::Determinant() const
{
	return FMatrixRegister::Load(*this).Determinant();
}

void FMatrix::SetColumn(int32 i, FVector4 Value)
{
	M[0][i] = Value[0];
	M[1][i] = Value[1];
	M[2][i] = Value[2];
	M[3][i] = Value[3];
}

void FMatrix::SetIdentity()
{
	M[0][0] = 1; M[0][1] = 0; M[0][2] = 0; M[0][3] = 0;
	M[1][0] = 0; M[1][1] = 1; M[1][2] = 0; M[1][3] = 0;
	M[2][0] = 0; M[2][1] = 0; M[2][2] = 1; M[2][3] = 0;
	M[3][0] = 0; M[3][1] = 0; M[3][2] = 0; M[3][3] = 1;
}

void FMatrix::SetOrigin(const FVector& NewOrigin)
{
	M[3][0] = NewOrigin.X;
	M[3][1] = NewOrigin.Y;
	M[3][2] = NewOrigin.Z;
}

FMatrix FMatrix::MakeTranslation(const FVector& T)
{
	FMatrix Mat;
	Mat.SetIdentity();
	Mat.M[3][0] = T.X;
	Mat.M[3][1] = T.Y;
	Mat.M[3][2] = T.Z;
	return Mat;
}

/* Operator */

FMatrix FMatrix::operator - ()
{
	FMatrixRegister M = FMatrixRegister::Load(*this);

	M.R[0] = VectorSIMD::Negate(M.R[0]);
	M.R[1] = VectorSIMD::Negate(M.R[1]);
	M.R[2] = VectorSIMD::Negate(M.R[2]);
	M.R[3] = VectorSIMD::Negate(M.R[3]);

	return M.ToFMatrix();
}

const float* FMatrix::operator[] (int32 Index) const
{
	return M[Index];
}

float* FMatrix::operator[] (int32 Index)
{
	return M[Index];
}

bool FMatrix::operator != (const FMatrix& Other) const
{
	return !(*this == Other);
}

bool FMatrix::operator == (const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	return
		VectorSIMD::IsNearlyEqual(A.R[0], B.R[0]) &&
		VectorSIMD::IsNearlyEqual(A.R[1], B.R[1]) &&
		VectorSIMD::IsNearlyEqual(A.R[2], B.R[2]) &&
		VectorSIMD::IsNearlyEqual(A.R[3], B.R[3]);
}


FMatrix FMatrix::operator * (const FMatrix& Other) const
{
	return Multiply(Other);
}

FMatrix FMatrix::operator * (const float& Other) const
{
	FMatrixRegister M = FMatrixRegister::Load(*this);

	FVectorRegister Scalar = VectorSIMD::SetVal(Other);

	M.R[0] = VectorSIMD::Mul(M.R[0], Scalar);
	M.R[1] = VectorSIMD::Mul(M.R[1], Scalar);
	M.R[2] = VectorSIMD::Mul(M.R[2], Scalar);
	M.R[3] = VectorSIMD::Mul(M.R[3], Scalar);

	return M.ToFMatrix();

}

FVector4 FMatrix::operator * (const FVector4& Other) const
{
	return TransformFVector4(Other);
}


FMatrix& FMatrix::operator *= (const FMatrix& Other)
{
	*this = *this * Other;
	return *this;
}

FMatrix& FMatrix::operator *= (float Other)
{
	*this = *this * Other;
	return *this;
}

FMatrix FMatrix::operator + (const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	FMatrixRegister Result;

	Result.R[0] = VectorSIMD::Add(A.R[0], B.R[0]);
	Result.R[1] = VectorSIMD::Add(A.R[1], B.R[1]);
	Result.R[2] = VectorSIMD::Add(A.R[2], B.R[2]);
	Result.R[3] = VectorSIMD::Add(A.R[3], B.R[3]);

	return Result.ToFMatrix();
}

FMatrix& FMatrix::operator += (const FMatrix& Other)
{
	*this = *this + Other;
	return *this;
}

FMatrix FMatrix::operator - (const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	FMatrixRegister Result;

	Result.R[0] = VectorSIMD::Sub(A.R[0], B.R[0]);
	Result.R[1] = VectorSIMD::Sub(A.R[1], B.R[1]);
	Result.R[2] = VectorSIMD::Sub(A.R[2], B.R[2]);
	Result.R[3] = VectorSIMD::Sub(A.R[3], B.R[3]);

	return Result.ToFMatrix();
}

FMatrix& FMatrix::operator -= (const FMatrix& Other)
{
	*this = *this - Other;
	return *this;
}


FMatrix FMatrix::Invert() const
{
	return Inverse();
}

FMatrix FMatrix::CreateScale(float X, float Y, float Z)
{
	return CreateScale(FVector(X, Y, Z));
}

FMatrix FMatrix::CreateScale(const FVector& Scale)
{
	FMatrix Result;
	Result.M[0][0] = Scale.X;
	Result.M[1][1] = Scale.Y;
	Result.M[2][2] = Scale.Z;
	return Result;
}

FMatrix FMatrix::CreateTranslation(float X, float Y, float Z)
{
	return CreateTranslation(FVector(X, Y, Z));
}

FMatrix FMatrix::CreateTranslation(const FVector& Position)
{
	FMatrix Result;
	Result.Translation(Position);
	return Result;
}

FMatrix FMatrix::CreateRotationX(float Radians)
{
	FMatrix Result;
	const float C = std::cos(Radians);
	const float S = std::sin(Radians);
	Result.M[1][1] = C; Result.M[1][2] = S;
	Result.M[2][1] = -S; Result.M[2][2] = C;
	return Result;
}

FMatrix FMatrix::CreateRotationY(float Radians)
{
	FMatrix Result;
	const float C = std::cos(Radians);
	const float S = std::sin(Radians);
	Result.M[0][0] = C; Result.M[0][2] = -S;
	Result.M[2][0] = S; Result.M[2][2] = C;
	return Result;
}

FMatrix FMatrix::CreateRotationZ(float Radians)
{
	FMatrix Result;
	const float C = std::cos(Radians);
	const float S = std::sin(Radians);
	Result.M[0][0] = C; Result.M[0][1] = S;
	Result.M[1][0] = -S; Result.M[1][1] = C;
	return Result;
}

FMatrix FMatrix::CreateFromYawPitchRoll(float Yaw, float Pitch, float Roll)
{
	return CreateRotationX(Roll) * CreateRotationY(Pitch) * CreateRotationZ(Yaw);
}

FVector FMatrix::TransformDirection(const FVector& Direction) const
{
	return FVector(
		Direction.X * M[0][0] + Direction.Y * M[1][0] + Direction.Z * M[2][0],
		Direction.X * M[0][1] + Direction.Y * M[1][1] + Direction.Z * M[2][1],
		Direction.X * M[0][2] + Direction.Y * M[1][2] + Direction.Z * M[2][2]);
}

FVector FMatrix::Translation() const
{
	return FVector(M[3][0], M[3][1], M[3][2]);
}

void FMatrix::Translation(const FVector& Position)
{
	M[3][0] = Position.X;
	M[3][1] = Position.Y;
	M[3][2] = Position.Z;
}

//FVector FMatrix::Right() const { return FVector(M[0][0], M[0][1], M[0][2]); }
//FVector FMatrix::Up() const { return FVector(M[2][0], M[2][1], M[2][2]); }
//FVector FMatrix::Forward() const { return FVector(M[1][0], M[1][1], M[1][2]); }

FVector FMatrix::Forward() const { return FVector(M[0][0], M[0][1], M[0][2]); } // X축
FVector FMatrix::Right()   const { return FVector(M[1][0], M[1][1], M[1][2]); } // Y축
FVector FMatrix::Up()      const { return FVector(M[2][0], M[2][1], M[2][2]); } // Z축



FMatrix FMatrix::CreatePerspectiveFieldOfView(float FovY, float AspectRatio, float NearZ, float FarZ)
{
	FMatrix Result;
	for (auto& Row : Result.M) for (float& Value : Row) Value = 0.0f;
	const float YScale = 1.0f / std::tan(FovY * 0.5f);
	const float XScale = YScale / AspectRatio;
	const float DepthScale = FarZ / (FarZ - NearZ);
	Result.M[0][0] = XScale;
	Result.M[1][1] = YScale;
	Result.M[2][2] = DepthScale;
	Result.M[2][3] = 1.0f;
	Result.M[3][2] = -NearZ * DepthScale;
	return Result;
}

bool FMatrix::TransformCoord(const FVector& Position, FVector& OutPosition) const
{
	const float X = Position.X * M[0][0] + Position.Y * M[1][0] + Position.Z * M[2][0] + M[3][0];
	const float Y = Position.X * M[0][1] + Position.Y * M[1][1] + Position.Z * M[2][1] + M[3][1];
	const float Z = Position.X * M[0][2] + Position.Y * M[1][2] + Position.Z * M[2][2] + M[3][2];
	const float W = Position.X * M[0][3] + Position.Y * M[1][3] + Position.Z * M[2][3] + M[3][3];
	if (std::abs(W) <= 1e-6f) return false;
	OutPosition = FVector(X / W, Y / W, Z / W);
	return true;
}

bool FMatrix::TryInverse(FMatrix& OutInverse) const
{
	if (std::abs(Determinant()) <= 1e-8f) return false;
	OutInverse = Inverse();
	return true;
}

FMatrix FMatrix::CreateOrthographic(float Width, float Height, float NearZ, float FarZ)
{
	FMatrix Result;
	Result.M[0][0] = 2.0f / Width;
	Result.M[1][1] = 2.0f / Height;
	Result.M[2][2] = 1.0f / (FarZ - NearZ);
	Result.M[3][2] = -NearZ / (FarZ - NearZ);
	return Result;
}

DirectX::SimpleMath::Matrix FMatrix::ToSimpleMath() const
{
	return {M[0][0], M[0][1], M[0][2], M[0][3], M[1][0], M[1][1], M[1][2], M[1][3], M[2][0], M[2][1], M[2][2], M[2][3], M[3][0], M[3][1], M[3][2], M[3][3]};
}

FMatrix FMatrix::CreateFromQuaternion(const FQuat& Rotation)
{
	FQuat Q = Rotation;
	Q.Normalize();
	const float XX = Q.X * Q.X, YY = Q.Y * Q.Y, ZZ = Q.Z * Q.Z;
	const float XY = Q.X * Q.Y, XZ = Q.X * Q.Z, YZ = Q.Y * Q.Z;
	const float XW = Q.X * Q.W, YW = Q.Y * Q.W, ZW = Q.Z * Q.W;
	FMatrix Result;
	Result.M[0][0] = 1.0f - 2.0f * (YY + ZZ); Result.M[0][1] = 2.0f * (XY + ZW); Result.M[0][2] = 2.0f * (XZ - YW);
	Result.M[1][0] = 2.0f * (XY - ZW); Result.M[1][1] = 1.0f - 2.0f * (XX + ZZ); Result.M[1][2] = 2.0f * (YZ + XW);
	Result.M[2][0] = 2.0f * (XZ + YW); Result.M[2][1] = 2.0f * (YZ - XW); Result.M[2][2] = 1.0f - 2.0f * (XX + YY);
	return Result;
}

FQuat FMatrix::ToQuaternion() const
{
	return FQuat::CreateFromRotationMatrix(*this);
}

bool FMatrix::Decompose(FVector& OutScale, FQuat& OutRotation, FVector& OutTranslation) const
{
	FVector AxisX(M[0][0], M[0][1], M[0][2]);
	FVector AxisY(M[1][0], M[1][1], M[1][2]);
	FVector AxisZ(M[2][0], M[2][1], M[2][2]);
	OutScale = FVector(AxisX.Length(), AxisY.Length(), AxisZ.Length());
	if (OutScale.X <= 1e-6f || OutScale.Y <= 1e-6f || OutScale.Z <= 1e-6f) return false;
	AxisX /= OutScale.X; AxisY /= OutScale.Y; AxisZ /= OutScale.Z;
	FMatrix Rotation;
	Rotation.SetAxis(0, AxisX); Rotation.SetAxis(1, AxisY); Rotation.SetAxis(2, AxisZ);
	OutRotation = Rotation.ToQuaternion();
	OutTranslation = Translation();
	return true;
}

FVector FVector::Transform(const FVector& Position, const FMatrix& Matrix)
{
	FVector Result;
	if (!Matrix.TransformCoord(Position, Result))
	{
		const float Invalid = std::numeric_limits<float>::quiet_NaN();
		return FVector(Invalid, Invalid, Invalid);
	}
	return Result;
}

FVector FVector::TransformNormal(const FVector& Direction, const FMatrix& Matrix)
{
	return Matrix.TransformDirection(Direction);
}

const FMatrix FMatrix::Identity = FMatrix(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

/* Global Operator */
std::ostream& operator << (std::ostream& OS, const FMatrix& M)
{
	OS << M[0][0] << " " << M[0][1] << " " << M[0][2] << " " << M[0][3] << "\n";
	OS << M[1][0] << " " << M[1][1] << " " << M[1][2] << " " << M[1][3] << "\n";
	OS << M[2][0] << " " << M[2][1] << " " << M[2][2] << " " << M[2][3] << "\n";
	OS << M[3][0] << " " << M[3][1] << " " << M[3][2] << " " << M[3][3] << "\n";
	return OS;
}

FRotator MatrixToRotator(const FMatrix& Mat)
{
	FRotator R;
	R.Roll = atan2f(Mat.M[1][2], Mat.M[2][2]) * 180.0f / PI;
	R.Pitch = atan2f(-Mat.M[0][2], sqrtf(Mat.M[1][2] * Mat.M[1][2] + Mat.M[2][2] * Mat.M[2][2])) * 180.0f / PI;
	R.Yaw = atan2f(Mat.M[0][1], Mat.M[0][0]) * 180.0f / PI;
	return R;
}
