#include "pch.h"
#include "Math/FMath.h"
#include "FTransform.h"
#include "Core/Archive/FArchive.h"

namespace {
    FMatrix MakeTransformMatrix(const FVector3& Position, const FQuat& Rotation, const FVector3& Scale) {
        FMatrix ScaleMatrix{FMatrix::CreateScale(Scale)};

        FMatrix RotationMatrix{FMatrix::CreateFromQuaternion(Rotation)};
        FMatrix Result{ScaleMatrix * RotationMatrix};

        const float Row0[3]{Result.m_[0][0], Result.m_[0][1], Result.m_[0][2]};
        const float Row1[3]{Result.m_[1][0], Result.m_[1][1], Result.m_[1][2]};
        const float Row2[3]{Result.m_[2][0], Result.m_[2][1], Result.m_[2][2]};
        //for (Uint32 Column = 0; Column < 3; ++Column) {
        //    Result.m[0][Column] = -Row0[Column];
        //    Result.m[1][Column] = Row2[Column];
        //    Result.m[2][Column] = Row1[Column];
        //}
        Result.Translation(Position);
        return Result;
    }
}

void FTransform::SetRotation(const FRotator& InRotation) {
    mRotationEuler = InRotation;
    mRotation = FQuat::FromRotator(InRotation);
}

void FTransform::SetRotation(const FQuat& InRotation) {
    mRotation = InRotation;
    mRotation.Normalize();
    mRotationEuler = mRotation.ToRotator();
}

FMatrix FTransform::ToMatrixWithScale() const {
    return MakeTransformMatrix(mPosition, mRotation, mScale);
}

FMatrix FTransform::ToMatrixNoScale() const {
    return MakeTransformMatrix(mPosition, mRotation, {1.0f, 1.0f, 1.0f});
}

FMatrix FTransform::ToInverseMatrixWithScale() const {
    return ToMatrixWithScale().Invert();
}

FTransform FTransform::Compose(const FTransform& Parent) const {
    const FVector3 ScaledPosition{ mPosition.mX * Parent.mScale.mX, mPosition.mY * Parent.mScale.mY, mPosition.mZ * Parent.mScale.mZ};

    const FVector3 WorldPosition{mBAbsoluteLocation ? mPosition : FMatrix::CreateFromQuaternion(Parent.mRotation).TransformDirection(ScaledPosition) + Parent.mPosition};

    const FVector3 WorldScale{ mScale.mX * Parent.mScale.mX, mScale.mY * Parent.mScale.mY, mScale.mZ * Parent.mScale.mZ};

    FTransform WorldTransform{ WorldPosition, mBAbsoluteRotation ? mRotation : FQuat::Concatenate(mRotation, Parent.mRotation), mBAbsoluteScale ? mScale : WorldScale};
    WorldTransform.SetAbsoluteLocation(mBAbsoluteLocation);
    WorldTransform.SetAbsoluteRotation(mBAbsoluteRotation);
    WorldTransform.SetAbsoluteScale(mBAbsoluteScale);
    return WorldTransform;
}

bool FTransform::MakeRelativeTo(const FTransform& Parent, FTransform& OutRelative) const {
    constexpr float Epsilon{1e-6f};
    if ((!mBAbsoluteLocation || !mBAbsoluteScale) &&
        (std::abs(Parent.mScale.mX) <= Epsilon || std::abs(Parent.mScale.mY) <= Epsilon || std::abs(Parent.mScale.mZ) <= Epsilon)) {
        return false;
    }

    FVector3 RelativePosition{mPosition};
    if (!mBAbsoluteLocation) {
        const FVector3 ParentSpacePosition{FMatrix::CreateFromQuaternion(Parent.mRotation.Inverse()).TransformDirection(mPosition - Parent.mPosition)};
        RelativePosition = { ParentSpacePosition.mX / Parent.mScale.mX, ParentSpacePosition.mY / Parent.mScale.mY, ParentSpacePosition.mZ / Parent.mScale.mZ};
    }

    FVector3 RelativeScale{mScale};
    if (!mBAbsoluteScale) {
        RelativeScale = { mScale.mX / Parent.mScale.mX, mScale.mY / Parent.mScale.mY, mScale.mZ / Parent.mScale.mZ};
    }

    OutRelative = { RelativePosition, mBAbsoluteRotation ? mRotation : FQuat::Concatenate(mRotation, Parent.mRotation.Inverse()), RelativeScale};
    OutRelative.SetAbsoluteLocation(mBAbsoluteLocation);
    OutRelative.SetAbsoluteRotation(mBAbsoluteRotation);
    OutRelative.SetAbsoluteScale(mBAbsoluteScale);
    return true;
}

void FTransform::Serialize(FArchive& Archive) {
    Archive.Serialize("Position", mPosition);
    Archive.Serialize("Rotation", mRotation);
    Archive.Serialize("Scale", mScale);
    Archive.Serialize("bAbsoluteLocation", mBAbsoluteLocation);
    Archive.Serialize("bAbsoluteRotation", mBAbsoluteRotation);
    Archive.Serialize("bAbsoluteScale", mBAbsoluteScale);

    if (Archive.IsLoading()) {
        SetRotation(mRotation);
    }
}
