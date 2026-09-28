/* #include "FMath.h"

#include <limits>

FVector2::FVector2(float InX, float InY)
    : mX(InX),
      mY(InY) {
}

FVector::FVector(float InX, float InY, float InZ)
    : mX(InX),
      mY(InY),
      mZ(InZ) {
}

FVector4::FVector4(float InX, float InY, float InZ, float InW)
    : mX(InX),
      mY(InY),
      mZ(InZ),
      mW(InW) {
}

FVector4::FVector4(FVector InXYZ, float InW)
    : mX(InXYZ.mX),
      mY(InXYZ.mY),
      mZ(InXYZ.mZ),
      mW(InW) {
}

FRotator::FRotator(float InPitch, float InYaw, float InRoll)
    : mX(InPitch),
      mY(InYaw),
      mZ(InRoll) {
}

FRotator::FRotator(const FVector& InEuler)
    : mX(InEuler.mX),
      mY(InEuler.mY),
      mZ(InEuler.mZ) {
}

FRotator::operator FVector() const {
    return FVector{mX, mY, mZ};
}

bool FRotator::operator==(const FVector& Other) const {
    return mX == Other.mX && mY == Other.mY && mZ == Other.mZ;
}

float FVector2::LengthSquared() const {
    return mX * mX + mY * mY;
}

float FVector2::Length() const {
    return std::sqrt(LengthSquared());
}

void FVector2::Normalize() {
    float VectorLength{Length()};
    if (VectorLength > 0.0f) {
        mX /= VectorLength;
        mY /= VectorLength;
    }
}

FVector2 FVector2::operator+(const FVector2& Other) const {
    return FVector2{mX + Other.mX, mY + Other.mY};
}

FVector2 FVector2::operator-(const FVector2& Other) const {
    return FVector2{mX - Other.mX, mY - Other.mY};
}

FVector2 FVector2::operator*(float Scalar) const {
    return FVector2{mX * Scalar, mY * Scalar};
}

FVector2& FVector2::operator/=(float Scalar) {
    mX /= Scalar;
    mY /= Scalar;
    return *this;
}

FVector2& FVector2::operator+=(const FVector2& Other) {
    mX += Other.mX;
    mY += Other.mY;
    return *this;
}

FVector::FVector(const DirectX::XMFLOAT3& Value)
    : mX(Value.x),
      mY(Value.y),
      mZ(Value.z) {
}

DirectX::SimpleMath::Vector3 FVector::ToSimpleMath() const {
    return {mX, mY, mZ};
}

FVector FVector::operator-() const {
    return FVector{-mX, -mY, -mZ};
}

FVector FVector::Min(const FVector& A, const FVector& B) {
    return {A.mX < B.mX ? A.mX : B.mX, A.mY < B.mY ? A.mY : B.mY, A.mZ < B.mZ ? A.mZ : B.mZ};
}

FVector FVector::Max(const FVector& A, const FVector& B) {
    return {A.mX > B.mX ? A.mX : B.mX, A.mY > B.mY ? A.mY : B.mY, A.mZ > B.mZ ? A.mZ : B.mZ};
}

FVector FVector::operator+(const FVector& Rhs) const {
    return FVector{mX + Rhs.mX, mY + Rhs.mY, mZ + Rhs.mZ};
}

FVector FVector::operator-(const FVector& Rhs) const {
    return FVector{mX - Rhs.mX, mY - Rhs.mY, mZ - Rhs.mZ};
}

FVector FVector::operator*(float Scalar) const {
    return FVector{mX * Scalar, mY * Scalar, mZ * Scalar};
}

FVector FVector::operator/(float Scalar) const {
    return FVector{mX / Scalar, mY / Scalar, mZ / Scalar};
}

float FVector::Dot(const FVector& Rhs) const {
    return mX * Rhs.mX + mY * Rhs.mY + mZ * Rhs.mZ;
}

FVector FVector::Cross(const FVector& Rhs) const {
    return FVector{mY * Rhs.mZ - mZ * Rhs.mY, mZ * Rhs.mX - mX * Rhs.mZ, mX * Rhs.mY - mY * Rhs.mX};
}

float FVector::LengthSquared() const {
    return Dot(*this);
}

float FVector::Length() const {
    return std::sqrt(LengthSquared());
}

void FVector::Normalize() {
    const float Magnitude{Length()};

    if (Magnitude <= 1e-6f) {
        mX = mY = mZ = 0.0f;
        return;
    }

    mX /= Magnitude;
    mY /= Magnitude;
    mZ /= Magnitude;
}

float FVector4::Dot(const FVector4& Rhs) const {
    return mX * Rhs.mX + mY * Rhs.mY + mZ * Rhs.mZ + mW * Rhs.mW;
}

float FVector4::LengthSquared() const {
    return Dot(*this);
}

float FVector4::Length() const {
    return std::sqrt(LengthSquared());
}

FVector4 FVector4::operator+(const FVector4& Rhs) const {
    return FVector4{mX + Rhs.mX, mY + Rhs.mY, mZ + Rhs.mZ, mW + Rhs.mW};
}

FVector4 FVector4::operator-(const FVector4& Rhs) const {
    return FVector4{mX - Rhs.mX, mY - Rhs.mY, mZ - Rhs.mZ, mW - Rhs.mW};
}

FVector4 FVector4::operator*(float Scalar) const {
    return FVector4{mX * Scalar, mY * Scalar, mZ * Scalar, mW * Scalar};
}

FVector4& FVector4::operator+=(const FVector4& Rhs) {
    mX += Rhs.mX;
    mY += Rhs.mY;
    mZ += Rhs.mZ;
    mW += Rhs.mW;
    return *this;
}

FVector4& FVector4::operator-=(const FVector4& Rhs) {
    mX -= Rhs.mX;
    mY -= Rhs.mY;
    mZ -= Rhs.mZ;
    mW -= Rhs.mW;
    return *this;
}

FVector4& FVector4::operator*=(float Scalar) {
    mX *= Scalar;
    mY *= Scalar;
    mZ *= Scalar;
    mW *= Scalar;
    return *this;
}

FVector4& FVector4::operator/=(float Scalar) {
    mX /= Scalar;
    mY /= Scalar;
    mZ /= Scalar;
    mW /= Scalar;
    return *this;
}

FMatrix FMatrix::Invert() const {
    FMatrix Result{};
    if (!TryInverse(Result))
        for (auto& Row : Result.m_)
            for (float& Value : Row)
                Value = std::numeric_limits<float>::infinity();
    return Result;
}

FMatrix FMatrix::CreateScale(float X, float Y, float Z) {
    return CreateScale(FVector{X, Y, Z});
}

FMatrix FMatrix::CreateTranslation(float X, float Y, float Z) {
    return CreateTranslation(FVector{X, Y, Z});
}

FMatrix FMatrix::operator*(const FMatrix& Rhs) const {
    FMatrix Result{};

    for (int Row{0}; Row < 4; ++Row) {
        for (int Col{0}; Col < 4; ++Col) {
            Result.m_[Row][Col] = 0.0f;

            for (int K{0}; K < 4; ++K) {
                Result.m_[Row][Col] += m_[Row][K] * Rhs.m_[K][Col];
            }
        }
    }

    return Result;
}

FMatrix FMatrix::CreateScale(const FVector& Scale) {
    FMatrix Result{};

    Result.m_[0][0] = Scale.mX;
    Result.m_[1][1] = Scale.mY;
    Result.m_[2][2] = Scale.mZ;

    return Result;
}

FMatrix FMatrix::CreateTranslation(const FVector& Position) {
    FMatrix Result{};

    Result.m_[3][0] = Position.mX;
    Result.m_[3][1] = Position.mY;
    Result.m_[3][2] = Position.mZ;

    return Result;
}

FMatrix FMatrix::CreateRotationX(float Radians) {
    FMatrix Result{};
    const float C{std::cos(Radians)};
    const float S{std::sin(Radians)};

    Result.m_[1][1] = C;
    Result.m_[1][2] = S;
    Result.m_[2][1] = -S;
    Result.m_[2][2] = C;

    return Result;
}

FMatrix FMatrix::CreateRotationY(float Radians) {
    FMatrix Result{};
    const float C{std::cos(Radians)};
    const float S{std::sin(Radians)};

    Result.m_[0][0] = C;
    Result.m_[0][2] = -S;
    Result.m_[2][0] = S;
    Result.m_[2][2] = C;

    return Result;
}

FMatrix FMatrix::CreateRotationZ(float Radians) {
    FMatrix Result{};
    const float C{std::cos(Radians)};
    const float S{std::sin(Radians)};

    Result.m_[0][0] = C;
    Result.m_[0][1] = S;
    Result.m_[1][0] = -S;
    Result.m_[1][1] = C;

    return Result;
}

FMatrix FMatrix::CreateFromYawPitchRoll(float Yaw, float Pitch, float Roll) {
    return CreateRotationY(Roll) * CreateRotationX(Pitch) * CreateRotationZ(Yaw);
}

FVector FMatrix::TransformPosition(const FVector& Position) const {
    return FVector{ Position.mX * m_[0][0] + Position.mY * m_[1][0] + Position.mZ * m_[2][0] + m_[3][0], Position.mX * m_[0][1] + Position.mY * m_[1][1] + Position.mZ * m_[2][1] + m_[3][1], Position.mX * m_[0][2] + Position.mY * m_[1][2] + Position.mZ * m_[2][2] + m_[3][2]};
}

FVector FMatrix::TransformDirection(const FVector& Direction) const {
    return FVector{ Direction.mX * m_[0][0] + Direction.mY * m_[1][0] + Direction.mZ * m_[2][0], Direction.mX * m_[0][1] + Direction.mY * m_[1][1] + Direction.mZ * m_[2][1], Direction.mX * m_[0][2] + Direction.mY * m_[1][2] + Direction.mZ * m_[2][2]};
}

FVector FMatrix::Translation() const {
    return FVector{m_[3][0], m_[3][1], m_[3][2]};
}

void FMatrix::Translation(const FVector& Position) {
    m_[3][0] = Position.mX;
    m_[3][1] = Position.mY;
    m_[3][2] = Position.mZ;
}

FVector FMatrix::Right() const {
    return FVector{m_[0][0], m_[0][1], m_[0][2]};
}

FVector FMatrix::Up() const {
    return FVector{m_[2][0], m_[2][1], m_[2][2]};
}

FVector FMatrix::Forward() const {
    return FVector{m_[1][0], m_[1][1], m_[1][2]};
}

FMatrix FMatrix::CreatePerspectiveFieldOfView(float FovY, float AspectRatio, float NearZ, float FarZ) {
    const float YScale{1.0f / std::tan(FovY * 0.5f)};
    const float XScale{YScale / AspectRatio};
    const float DepthScale{FarZ / (FarZ - NearZ)};

    FMatrix Result{};

    for (int Row{0}; Row < 4; ++Row) {
        for (int Col{0}; Col < 4; ++Col) {
            Result.m_[Row][Col] = 0.0f;
        }
    }

    Result.m_[0][0] = XScale;
    Result.m_[1][1] = YScale;
    Result.m_[2][2] = DepthScale;
    Result.m_[2][3] = 1.0f;
    Result.m_[3][2] = -NearZ * DepthScale;

    return Result;
}

bool FMatrix::TransformCoord(const FVector& Position, FVector& OutPosition) const {
    const float X{Position.mX * m_[0][0] + Position.mY * m_[1][0] + Position.mZ * m_[2][0] + m_[3][0]};

    const float Y{Position.mX * m_[0][1] + Position.mY * m_[1][1] + Position.mZ * m_[2][1] + m_[3][1]};

    const float Z{Position.mX * m_[0][2] + Position.mY * m_[1][2] + Position.mZ * m_[2][2] + m_[3][2]};

    const float W{Position.mX * m_[0][3] + Position.mY * m_[1][3] + Position.mZ * m_[2][3] + m_[3][3]};

    if (std::abs(W) <= 1e-6f) {
        return false;
    }

    OutPosition = FVector{X / W, Y / W, Z / W};
    return true;
}

bool FMatrix::TryInverse(FMatrix& OutInverse) const {
    double Augmented[4][8]{};

    for (int Row{0}; Row < 4; ++Row) {
        for (int Col{0}; Col < 4; ++Col) {
            Augmented[Row][Col] = m_[Row][Col];
        }

        Augmented[Row][Row + 4] = 1.0;
    }

    for (int Col{0}; Col < 4; ++Col) {
        int PivotRow{Col};

        for (int Row{Col + 1}; Row < 4; ++Row) {
            if (std::abs(Augmented[Row][Col]) >
                std::abs(Augmented[PivotRow][Col])) {
                PivotRow = Row;
            }
        }

        if (std::abs(Augmented[PivotRow][Col]) <= 1e-12) {
            return false;
        }

        if (PivotRow != Col) {
            for (int K{0}; K < 8; ++K) {
                const double Temp{Augmented[Col][K]};
                Augmented[Col][K] = Augmented[PivotRow][K];
                Augmented[PivotRow][K] = Temp;
            }
        }

        const double Pivot{Augmented[Col][Col]};

        for (int K{0}; K < 8; ++K) {
            Augmented[Col][K] /= Pivot;
        }

        for (int Row{0}; Row < 4; ++Row) {
            if (Row == Col) {
                continue;
            }

            const double Factor{Augmented[Row][Col]};

            for (int K{0}; K < 8; ++K) {
                Augmented[Row][K] -= Factor * Augmented[Col][K];
            }
        }
    }

    FMatrix Result{};

    for (int Row{0}; Row < 4; ++Row) {
        for (int Col{0}; Col < 4; ++Col) {
            Result.m_[Row][Col] = static_cast<float>(Augmented[Row][Col + 4]);
        }
    }

    OutInverse = Result;
    return true;
}

FMatrix FMatrix::CreateOrthographic(float Width, float Height, float NearZ, float FarZ) {
    FMatrix Result{};

    Result.m_[0][0] = 2.0f / Width;
    Result.m_[1][1] = 2.0f / Height;
    Result.m_[2][2] = 1.0f / (FarZ - NearZ);
    Result.m_[3][2] = -NearZ / (FarZ - NearZ);

    return Result;
}

DirectX::SimpleMath::Matrix FMatrix::ToSimpleMath() const {
    return {m_[0][0], m_[0][1], m_[0][2], m_[0][3], m_[1][0], m_[1][1], m_[1][2], m_[1][3], m_[2][0], m_[2][1], m_[2][2], m_[2][3], m_[3][0], m_[3][1], m_[3][2], m_[3][3]};
}

FQuat::FQuat()
    : mX(0),
      mY(0),
      mZ(0),
      mW(1) {
}

FQuat::FQuat(float X, float Y, float Z, float W)
    : mX(X),
      mY(Y),
      mZ(Z),
      mW(W) {
}

FQuat::FQuat(const FVector& V, float W)
    : mX(V.mX),
      mY(V.mY),
      mZ(V.mZ),
      mW(W) {
}

FQuat::FQuat(const FVector4& V)
    : mX(V.mX),
      mY(V.mY),
      mZ(V.mZ),
      mW(V.mW) {
}

FQuat::FQuat(const DirectX::XMFLOAT4& Value)
    : mX(Value.x),
      mY(Value.y),
      mZ(Value.z),
      mW(Value.w) {
}

DirectX::SimpleMath::Quaternion FQuat::ToSimpleMath() const {
    return {mX, mY, mZ, mW};
}

FQuat FQuat::operator*(const FQuat& Other) const {
    return FQuat{
        mW * Other.mX + mX * Other.mW + mY * Other.mZ - mZ * Other.mY, // X
        mW * Other.mY - mX * Other.mZ + mY * Other.mW + mZ * Other.mX, // Y
        mW * Other.mZ + mX * Other.mY - mY * Other.mX + mZ * Other.mW, // Z
        mW * Other.mW - mX * Other.mX - mY * Other.mY - mZ * Other.mZ  // W
    };
}

FQuat FQuat::FromRotator(const FRotator& Rotation) {
    const FQuat Roll{CreateFromAxisAngle(FVector::UnitY, Rotation.mZ)};
    const FQuat Pitch{CreateFromAxisAngle(FVector::UnitX, Rotation.mX)};
    const FQuat Yaw{CreateFromAxisAngle(FVector::UnitZ, Rotation.mY)};
    return Concatenate(Concatenate(Roll, Pitch), Yaw);
}

FRotator FQuat::ToRotator() const {
    const FMatrix RotationMatrix{FMatrix::CreateFromQuaternion(*this)};
    const float Pitch{std::asin(std::clamp(RotationMatrix.m_[1][2], -1.0f, 1.0f))};
    const float CosPitch{std::cos(Pitch)};

    if (std::abs(CosPitch) > 1e-6f) {
        return { Pitch, std::atan2(-RotationMatrix.m_[1][0], RotationMatrix.m_[1][1]), std::atan2(-RotationMatrix.m_[0][2], RotationMatrix.m_[2][2])};
    }

    return {Pitch, std::atan2(RotationMatrix.m_[0][1], RotationMatrix.m_[0][0]), 0.0f};
}

void FQuat::Normalize() {
    float D{std::sqrt(mX * mX + mY * mY + mZ * mZ + mW * mW)};
    if (D <= 1e-8f) {
        *this = FQuat{};
        return;
    }
    mX /= D;
    mY /= D;
    mZ /= D;
    mW /= D;
}

FQuat FQuat::Inverse() const {
    DirectX::SimpleMath::Quaternion Result{};
    ToSimpleMath().Inverse(Result);
    return FQuat{Result};
}

FQuat FQuat::Concatenate(const FQuat& First, const FQuat& Second) {
    return First * Second;
}

const FQuat FQuat::CreateFromRotationMatrix(const FMatrix& M) {
    FQuat Result{};
    float R22{M.m_[2][2]};
    if (R22 <= 0.f) // x^2 + y^2 >= z^2 + w^2
    {
        float Dif10{M.m_[1][1] - M.m_[0][0]};
        float Omr22{1.f - R22};
        if (Dif10 <= 0.f) // x^2 >= y^2
        {
            float FourXSqr{Omr22 - Dif10};
            float Inv4x{0.5f / sqrtf(FourXSqr)};
            Result.mX = FourXSqr * Inv4x;
            Result.mY = (M.m_[0][1] + M.m_[1][0]) * Inv4x;
            Result.mZ = (M.m_[0][2] + M.m_[2][0]) * Inv4x;
            Result.mW = (M.m_[1][2] - M.m_[2][1]) * Inv4x;
        } else // y^2 >= x^2
        {
            float FourYSqr{Omr22 + Dif10};
            float Inv4y{0.5f / sqrtf(FourYSqr)};
            Result.mX = (M.m_[0][1] + M.m_[1][0]) * Inv4y;
            Result.mY = FourYSqr * Inv4y;
            Result.mZ = (M.m_[1][2] + M.m_[2][1]) * Inv4y;
            Result.mW = (M.m_[2][0] - M.m_[0][2]) * Inv4y;
        }
    } else // z^2 + w^2 >= x^2 + y^2
    {
        float Sum10{M.m_[1][1] + M.m_[0][0]};
        float Opr22{1.f + R22};
        if (Sum10 <= 0.f) // z^2 >= w^2
        {
            float FourZSqr{Opr22 - Sum10};
            float Inv4z{0.5f / sqrtf(FourZSqr)};
            Result.mX = (M.m_[0][2] + M.m_[2][0]) * Inv4z;
            Result.mY = (M.m_[1][2] + M.m_[2][1]) * Inv4z;
            Result.mZ = FourZSqr * Inv4z;
            Result.mW = (M.m_[0][1] - M.m_[1][0]) * Inv4z;
        } else // w^2 >= z^2
        {
            float FourWSqr{Opr22 + Sum10};
            float Inv4w{0.5f / sqrtf(FourWSqr)};
            Result.mX = (M.m_[1][2] - M.m_[2][1]) * Inv4w;
            Result.mY = (M.m_[2][0] - M.m_[0][2]) * Inv4w;
            Result.mZ = (M.m_[0][1] - M.m_[1][0]) * Inv4w;
            Result.mW = FourWSqr * Inv4w;
        }
    }
    return Result;
}

const FVector FQuat::ToEuler() const {
    const float Xx{mX * mX};
    const float Yy{mY * mY};
    const float Zz{mZ * mZ};

    const float M31{2.f * mX * mZ + 2.f * mY * mW};
    const float M32{2.f * mY * mZ - 2.f * mX * mW};
    const float M33{1.f - 2.f * Xx - 2.f * Yy};

    const float Cy{sqrtf(M33 * M33 + M31 * M31)};
    const float Cx{atan2f(-M32, Cy)};
    if (Cy > 16.f * FLT_EPSILON) {
        const float M12{2.f * mX * mY + 2.f * mZ * mW};
        const float M22{1.f - 2.f * Xx - 2.f * Zz};

        return FVector{Cx, atan2f(M31, M33), atan2f(M12, M22)};
    } else {
        const float M11{1.f - 2.f * Yy - 2.f * Zz};
        const float M21{2.f * mX * mY - 2.f * mZ * mW};

        return FVector{Cx, 0.f, atan2f(-M21, M11)};
    }
}

const FQuat FQuat::CreateFromAxisAngle(const FVector Axis, const float Angle) {
    FVector Result{Axis};
    Result.Normalize();

    float HalfAngle{Angle * 0.5f};

    float S{std::sin(HalfAngle)};
    float C{std::cos(HalfAngle)};

    return FQuat{Result.mX * S, Result.mY * S, Result.mZ * S, C};
}

FVector FVector::Transform(const FVector& Position, const FMatrix& Matrix) {
    FVector Result{};
    if (!Matrix.TransformCoord(Position, Result)) {
        const float Invalid{std::numeric_limits<float>::quiet_NaN()};
        return {Invalid, Invalid, Invalid};
    }
    return Result;
}

FVector FVector::TransformNormal(const FVector& Direction, const FMatrix& Matrix) {
    return Matrix.TransformDirection(Direction);
}

FQuat FMatrix::ToQuaternion() const {
    return FQuat::CreateFromRotationMatrix(*this);
}

bool FMatrix::Decompose(FVector& OutScale, FQuat& OutRotation, FVector& OutTranslation) const {
    constexpr float Epsilon{1e-6f};
    constexpr float OrthogonalTolerance{1e-4f};

    if (std::abs(m_[0][3]) > Epsilon ||
        std::abs(m_[1][3]) > Epsilon ||
        std::abs(m_[2][3]) > Epsilon ||
        std::abs(m_[3][3] - 1.0f) > Epsilon) {
        return false;
    }

    FVector AxisX{m_[0][0], m_[0][1], m_[0][2]};
    FVector AxisY{m_[1][0], m_[1][1], m_[1][2]};
    FVector AxisZ{m_[2][0], m_[2][1], m_[2][2]};

    FVector Scale{ AxisX.Length(), AxisY.Length(), AxisZ.Length()};

    if (Scale.mX <= Epsilon ||
        Scale.mY <= Epsilon ||
        Scale.mZ <= Epsilon) {
        return false;
    }

    AxisX = AxisX / Scale.mX;
    AxisY = AxisY / Scale.mY;
    AxisZ = AxisZ / Scale.mZ;

    if (std::abs(AxisX.Dot(AxisY)) > OrthogonalTolerance ||
        std::abs(AxisX.Dot(AxisZ)) > OrthogonalTolerance ||
        std::abs(AxisY.Dot(AxisZ)) > OrthogonalTolerance) {
        return false;
    }

    const float Determinant{AxisX.Dot(AxisY.Cross(AxisZ))};

    if (Determinant < 0.0f) {
        Scale.mX = -Scale.mX;
        AxisX = AxisX * -1.0f;
    }

    FMatrix RotationMatrix{};

    RotationMatrix.m_[0][0] = AxisX.mX;
    RotationMatrix.m_[0][1] = AxisX.mY;
    RotationMatrix.m_[0][2] = AxisX.mZ;

    RotationMatrix.m_[1][0] = AxisY.mX;
    RotationMatrix.m_[1][1] = AxisY.mY;
    RotationMatrix.m_[1][2] = AxisY.mZ;

    RotationMatrix.m_[2][0] = AxisZ.mX;
    RotationMatrix.m_[2][1] = AxisZ.mY;
    RotationMatrix.m_[2][2] = AxisZ.mZ;

    OutScale = Scale;
    OutRotation = RotationMatrix.ToQuaternion();
    OutTranslation = Translation();

    return true;
}

FMatrix FMatrix::CreateFromQuaternion(const FQuat& Rotation) {
    FQuat Q{Rotation};
    Q.Normalize();

    const float Xx{Q.mX * Q.mX};
    const float Yy{Q.mY * Q.mY};
    const float Zz{Q.mZ * Q.mZ};

    const float Xy{Q.mX * Q.mY};
    const float Xz{Q.mX * Q.mZ};
    const float Yz{Q.mY * Q.mZ};

    const float Xw{Q.mX * Q.mW};
    const float Yw{Q.mY * Q.mW};
    const float Zw{Q.mZ * Q.mW};

    FMatrix Result{};

    Result.m_[0][0] = 1.0f - 2.0f * (Yy + Zz);
    Result.m_[0][1] = 2.0f * (Xy + Zw);
    Result.m_[0][2] = 2.0f * (Xz - Yw);

    Result.m_[1][0] = 2.0f * (Xy - Zw);
    Result.m_[1][1] = 1.0f - 2.0f * (Xx + Zz);
    Result.m_[1][2] = 2.0f * (Yz + Xw);

    Result.m_[2][0] = 2.0f * (Xz + Yw);
    Result.m_[2][1] = 2.0f * (Yz - Xw);
    Result.m_[2][2] = 1.0f - 2.0f * (Xx + Yy);

    return Result;
}
*/