#include "pch.h"
#include "FTransform.h"

FTransform::FTransform(const FVector3& InPosition, const FRotator& InRotation, const FVector3& InScale)
	: mPosition{InPosition},
	  mRotation{FQuat::FromRotator(InRotation)},
	  mRotationEuler{InRotation},
	  mScale{InScale} {
}

FTransform::FTransform(const FVector3& InPosition, const FQuat& InRotation, const FVector3& InScale)
	: mPosition{InPosition},
	  mRotation{InRotation},
	  mScale{InScale} {
    mRotation.Normalize();
    mRotationEuler = mRotation.ToRotator();
}

FTransform& FTransform::operator=(const FTransform& Other) {
    if (this == &Other || HasSameState(Other)) {
        return *this;
    }

    const bool PositionOrRotationChanged{mPosition != Other.mPosition || mRotation.X != Other.mRotation.X || mRotation.Y != Other.mRotation.Y || mRotation.Z != Other.mRotation.Z || mRotation.W != Other.mRotation.W};
    const bool ScaleChanged{mScale != Other.mScale};
    mPosition = Other.mPosition;
    mRotation = Other.mRotation;
    mRotationEuler = Other.mRotationEuler;
    mScale = Other.mScale;
    mBAbsoluteLocation = Other.mBAbsoluteLocation;
    mBAbsoluteRotation = Other.mBAbsoluteRotation;
    mBAbsoluteScale = Other.mBAbsoluteScale;
    if (PositionOrRotationChanged || ScaleChanged) {
        InvalidateMatrices(PositionOrRotationChanged);
    }
    ++mRevision;
    return *this;
}

FTransform& FTransform::operator=(FTransform&& Other) noexcept {
    return operator=(static_cast<const FTransform&>(Other));
}

Uint64 FTransform::GetRevision() const {
    return mRevision;
}

const FVector3& FTransform::GetPosition() const {
    return mPosition;
}

const FVector3& FTransform::GetLocation() const {
    return mPosition;
}

const FRotator& FTransform::GetRotation() const {
    return mRotationEuler;
}

const FQuat& FTransform::GetRotationQuaternion() const {
    return mRotation;
}

const FVector3& FTransform::GetScale() const {
    return mScale;
}

const FVector3& FTransform::GetScale3D() const {
    return mScale;
}

bool FTransform::IsAbsoluteLocation() const {
    return mBAbsoluteLocation;
}

bool FTransform::IsAbsoluteRotation() const {
    return mBAbsoluteRotation;
}

bool FTransform::IsAbsoluteScale() const {
    return mBAbsoluteScale;
}

void FTransform::SetPosition(const FVector3& InPosition) {
    if (mPosition == InPosition) {
        return;
    }

    mPosition = InPosition;
    InvalidateMatrices(true);
    ++mRevision;
}

void FTransform::SetLocation(const FVector3& Location) {
    SetPosition(Location);
}

void FTransform::SetScale(const FVector3& InScale) {
    if (mScale == InScale) {
        return;
    }

    mScale = InScale;
    InvalidateMatrices(false);
    ++mRevision;
}

void FTransform::SetScale3D(const FVector3& Scale) {
    SetScale(Scale);
}

void FTransform::SetAbsoluteLocation(bool BInAbsoluteLocation) {
    if (mBAbsoluteLocation == BInAbsoluteLocation) {
        return;
    }

    mBAbsoluteLocation = BInAbsoluteLocation;
    ++mRevision;
}

void FTransform::SetAbsoluteRotation(bool BInAbsoluteRotation) {
    if (mBAbsoluteRotation == BInAbsoluteRotation) {
        return;
    }

    mBAbsoluteRotation = BInAbsoluteRotation;
    ++mRevision;
}

void FTransform::SetAbsoluteScale(bool BInAbsoluteScale) {
    if (mBAbsoluteScale == BInAbsoluteScale) {
        return;
    }

    mBAbsoluteScale = BInAbsoluteScale;
    ++mRevision;
}

bool FTransform::HasSameState(const FTransform& Other) const {
    return mPosition == Other.mPosition && mRotation.X == Other.mRotation.X && mRotation.Y == Other.mRotation.Y && mRotation.Z == Other.mRotation.Z && mRotation.W == Other.mRotation.W && mRotationEuler.Pitch == Other.mRotationEuler.Pitch && mRotationEuler.Yaw == Other.mRotationEuler.Yaw && mRotationEuler.Roll == Other.mRotationEuler.Roll && mScale == Other.mScale && mBAbsoluteLocation == Other.mBAbsoluteLocation && mBAbsoluteRotation == Other.mBAbsoluteRotation && mBAbsoluteScale == Other.mBAbsoluteScale;
}

void FTransform::InvalidateMatrices(bool IncludeNoScale) {
    mMatrixWithScaleDirty = true;
    mInverseMatrixWithScaleDirty = true;
    if (IncludeNoScale) {
        mMatrixNoScaleDirty = true;
    }
}
