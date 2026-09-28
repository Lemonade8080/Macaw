#pragma once

#include <iostream>
#include "Core/Types.h"
#include "Math/SimpleMath/SimpleMath.h"


struct FMatrix;
struct FVector {

	union
	{
		float V[3];

		struct
		{
			float X;
			float Y;
			float Z;
		};

		struct
		{
			float mX;
			float mY;
			float mZ;
		};
	};

public:
	/* Constructor */
	FVector();
	FVector(float x, float y, float z);
	explicit FVector(float f);
	explicit FVector(const DirectX::XMFLOAT3& Value);
	// ~FVector();

public:
	/* Public Functions */
	void Set(float x, float y, float z);

	float Size() const; // 길이 반환
	float Length() const; // ==  size()
	float Dot(const FVector& V1) const;

	float& Component(int32 index);		// 참조자 반환으로 lvalue로 직접 값수정 가능
	float Component(int32 index) const;

	FVector Add(const FVector& V1) const;
	FVector Subtract(const FVector& V1) const;

	FVector Cross(const FVector& V1) const;
	FVector GetAbs() const;
	void Normalize();

	static const FVector ZeroVector;    // (0, 0, 0)
	static const FVector OneVector;     // (1, 1, 1)

	float LengthSquared() const;
	DirectX::SimpleMath::Vector3 ToSimpleMath() const;

	// float GetMax();
	//// float GetMin();
	// float GetAbsMax();
	// float GetAbsMin();

	// GetSafeNormal
	// IsNearlyZero
	// Equals
	// ClampSize


/* operator */

	FVector operator - ();
	FVector operator - () const;

	FVector operator - (const FVector& V1) const;
	FVector& operator -= (const FVector& V1);

	FVector operator + (const FVector& V1) const;
	FVector& operator += (const FVector& V1);

	FVector operator * (const FVector& V1) const;
	FVector operator * (const float& f) const;
	FVector& operator *= (const FVector& V1);
	FVector& operator *= (const float& f);

	FVector operator / (const FVector& V1) const;
	FVector operator / (const float& f) const;
	FVector& operator /= (const FVector& V1);
	FVector& operator /= (const float& f);

	FVector operator ^ (const FVector& V1) const;

	bool operator == (const FVector& V1) const;
	bool operator != (const FVector& V1) const;

	float operator[] (int32 Index) const;
	float& operator[] (int32 Index);

	/* Static */
	static float Dot(const FVector& V1, const FVector& V2);
	static FVector Cross(const FVector& V1, const FVector& V2);
	static float Distance(const FVector& V1, const FVector& V2); // == Dist()
	/*static FVector DegreesToRadians(const FVector& V1);
	static FVector RadiansToDegrees(const FVector& V1);
	static FVector Max(const FVector& V1, const FVector& V2);
	static FVector Max3(const FVector& V1, const FVector& V2, const FVector& V3);
	static FVector Min(const FVector& V1, const FVector& V2);
	static FVector Min3(const FVector& V1, const FVector& V2, const FVector& V3);
	*/

	static const FVector Zero;
	static const FVector UnitX;
	static const FVector UnitY;
	static const FVector UnitZ;

	static FVector Min(const FVector& A, const FVector& B);
	static FVector Max(const FVector& A, const FVector& B);
	static FVector Transform(const FVector& Position, const FMatrix& Matrix);
	static FVector TransformNormal(const FVector& Direction, const FMatrix& Matrix);

};

using FVector3 = FVector;

inline const FVector FVector::Zero{};
inline const FVector FVector::UnitX{1.0f, 0.0f, 0.0f};
inline const FVector FVector::UnitY{0.0f, 1.0f, 0.0f};
inline const FVector FVector::UnitZ{0.0f, 0.0f, 1.0f};
inline const FVector FVector::ZeroVector{};
inline const FVector FVector::OneVector{1.0f, 1.0f, 1.0f};

/* Global Operator */
std::ostream& operator<<(std::ostream& OS, const FVector& V);

/* constants */
inline static const FVector BackwardVector = FVector(-1.0f, 0.0f, 0.0f);
inline static const FVector DownVector = FVector(0.0f, 0.0f, -1.0f);
inline static const FVector ForwardVector = FVector(1.0f, 0.0f, 0.0f);
inline static const FVector LeftVector = FVector(0.0f, -1.0f, 0.0f);
inline static const FVector OneVector = FVector(1.0f, 1.0f, 1.0f);
inline static const FVector RightVector = FVector(0.0f, 1.0f, 0.0f);
inline static const FVector UpVector = FVector(0.0f, 0.0f, 1.0f);
inline static const FVector XAxisVector = FVector(1.0f, 0.0f, 0.0f);
inline static const FVector YAxisVector = FVector(0.0f, 1.0f, 0.0f);
inline static const FVector ZAxisVector = FVector(0.0f, 0.0f, 1.0f);
inline static const FVector ZeroVector = FVector(0.0f, 0.0f, 0.0f);

/*#pragma once

#include <cmath>
#include "SimpleMath/SimpleMath.h"

struct FVector2
{
	float x = 0.0f;
	float y = 0.0f;

	constexpr FVector2() = default;
	constexpr FVector2(float InX, float InY) : x(InX), y(InY) {}

	float LengthSquared() const
	{
		return x * x + y * y;
	}

	float Length() const
	{
		return sqrt(LengthSquared());
	}

	void Normalize()
	{
		float VectorLength = Length();
		if (VectorLength > 0.0f)
		{
			x /= VectorLength;
			y /= VectorLength;
		}
	}

	FVector2 operator+(const FVector2& Other) const
	{
		return FVector2(x + Other.x, y + Other.y);
	}

	FVector2 operator-(const FVector2& Other) const
	{
		return FVector2(x - Other.x, y - Other.y);
	}

	FVector2 operator*(float Scalar) const
	{
		return FVector2(x * Scalar, y * Scalar);
	}

	FVector2& operator/=(float Scalar)
	{
		x /= Scalar;
		y /= Scalar;
		return *this;
	}
};

struct FVector
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	constexpr FVector() = default;

	constexpr FVector(float InX, float InY, float InZ) : x(InX), y(InY), z(InZ) {}

    explicit FVector(const DirectX::XMFLOAT3& value) : x(value.x), y(value.y), z(value.z) {}
    DirectX::SimpleMath::Vector3 ToSimpleMath() const { return {x, y, z}; }

    static FVector Transform(const FVector& position, const FMatrix& matrix);
    static FVector TransformNormal(const FVector& direction, const FMatrix& matrix);
	static const FVector Zero, UnitX, UnitY, UnitZ;
	bool operator==(const FVector&) const = default;
	FVector operator-() const { return FVector(-x, -y, -z); }
	static FVector Min(const FVector& a, const FVector& b)
	{
		return { a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z };
	}
	static FVector Max(const FVector& a, const FVector& b)
	{
		return { a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z };
	}

	FVector operator+(const FVector& rhs) const
	{
		return FVector(x + rhs.x, y + rhs.y, z + rhs.z);
	}

	FVector operator-(const FVector& rhs) const
	{
		return FVector(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	FVector operator*(float scalar) const
	{
		return FVector(x * scalar, y * scalar, z * scalar);
	}

	FVector operator/(float scalar) const
	{
		return FVector(x / scalar, y / scalar, z / scalar);
	}

	float Dot(const FVector& rhs) const {
		return x * rhs.x + y * rhs.y + z * rhs.z;
	}

	FVector Cross(const FVector& rhs) const {
		return FVector(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
	}

	float LengthSquared() const
	{
		return Dot(*this);
	}

	float Length() const
	{
		return std::sqrt(LengthSquared());
	}

	void Normalize()
	{
		const float length = Length();

		if (length <= 1e-6f)
		{
			x = y = z = 0.0f;
			return;
		}

		x /= length;
		y /= length;
		z /= length;
	}
};

inline const FVector FVector::Zero{};
inline const FVector FVector::UnitX{1, 0, 0};
inline const FVector FVector::UnitY{0, 1, 0};
inline const FVector FVector::UnitZ{0, 0, 1};

struct FVector4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	constexpr FVector4() = default;
	constexpr FVector4(float InX, float InY, float InZ, float InW) : x(InX), y(InY), z(InZ), w(InW) {}
	constexpr FVector4(FVector InXYZ, float InW) : x(InXYZ.x), y(InXYZ.y), z(InXYZ.z), w(InW) {}

	float Dot(const FVector4& Rhs) const
	{
		return x * Rhs.x + y * Rhs.y + z * Rhs.z+ w * Rhs.w;
	}

	float LengthSquared() const
	{
		return Dot(*this);
	}
	float Length() const
	{
		return std::sqrt(LengthSquared());
	}
	//float Length3Squared() const;
	//float Length3() const;

	FVector4 operator+(const FVector4& Rhs) const
	{
		return FVector4(x + Rhs.x, y + Rhs.y, z + Rhs.z, w + Rhs.w);
	}

	FVector4 operator-(const FVector4& Rhs) const
	{
		return FVector4(x - Rhs.x, y - Rhs.y, z - Rhs.z, w - Rhs.w);
	}

	FVector4 operator*(float Scalar) const
	{
		return FVector4(x * Scalar, y * Scalar, z * Scalar, w * Scalar);
	}

	FVector4& operator+=(const FVector4& Rhs)
	{
		x += Rhs.x;
		y += Rhs.y;
		z += Rhs.z;
		w += Rhs.w;
		return *this;
	}

	FVector4& operator-=(const FVector4& Rhs)
	{
		x -= Rhs.x;
		y -= Rhs.y;
		z -= Rhs.z;
		w -= Rhs.w;
		return *this;
	}

	FVector4& operator*=(float Scalar)
	{
		x *= Scalar;
		y *= Scalar;
		z *= Scalar;
		w *= Scalar;
		return *this;
	}

	FVector4& operator/=(float Scalar)
	{
		x /= Scalar;
		y /= Scalar;
		z /= Scalar;
		w /= Scalar;
		return *this;
	}
};*/
