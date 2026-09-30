#pragma once

#include "Math/FMath.h"
#include "Core/Archive/FArchive.h"

struct FTransform {
public:
    FTransform() = default;

    FTransform(const FVector3& InPosition, const FRotator& InRotation, const FVector3& InScale);

    FTransform(const FVector3& InPosition, const FQuat& InRotation, const FVector3& InScale);

    FTransform(const FTransform& Other) = default;
    FTransform& operator=(const FTransform& Other);
    FTransform(FTransform&& Other) noexcept = default;
    FTransform& operator=(FTransform&& Other) noexcept;

public:
    Uint64 GetRevision() const;

    const FVector3& GetPosition() const;

    const FVector3& GetLocation() const;

    const FRotator& GetRotation() const;

    const FQuat& GetRotationQuaternion() const;

    const FVector3& GetScale() const;

    const FVector3& GetScale3D() const;

    bool IsAbsoluteLocation() const;

    bool IsAbsoluteRotation() const;

    bool IsAbsoluteScale() const;

    void SetPosition(const FVector3& InPosition);

    void SetLocation(const FVector3& Location);

    void SetRotation(const FRotator& InRotation);
    void SetRotation(const FQuat& InRotation);

    void SetScale(const FVector3& InScale);

    void SetScale3D(const FVector3& Scale);

    void SetAbsoluteLocation(bool BInAbsoluteLocation);

    void SetAbsoluteRotation(bool BInAbsoluteRotation);

    void SetAbsoluteScale(bool BInAbsoluteScale);

    FMatrix ToMatrixWithScale() const;
    FMatrix ToMatrixNoScale() const;
    FMatrix ToInverseMatrixWithScale() const;
    FTransform Compose(const FTransform& Parent) const;
    bool MakeRelativeTo(const FTransform& Parent, FTransform& OutRelative) const;
    void Serialize(FArchive& Archive);

private:
    bool HasSameState(const FTransform& Other) const;
    void InvalidateMatrices(bool IncludeNoScale);

private:
    FVector3 mPosition{0.0f, 0.0f, 0.0f};
    FQuat mRotation{};
    FRotator mRotationEuler{};
    FVector3 mScale{1.0f, 1.0f, 1.0f};
    bool mBAbsoluteLocation{false};
    bool mBAbsoluteRotation{false};
    bool mBAbsoluteScale{false};
    Uint64 mRevision{};
    mutable FMatrix mMatrixWithScale{};
    mutable FMatrix mMatrixNoScale{};
    mutable FMatrix mInverseMatrixWithScale{};
    mutable bool mMatrixWithScaleDirty{true};
    mutable bool mMatrixNoScaleDirty{true};
    mutable bool mInverseMatrixWithScaleDirty{true};
};
