#include "pch.h"
#include "FLineRenderData.h"

void FLineRenderData::AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
    AddGridLine(Start, End, Color, WidthPixels, 0.0f, DepthMode);
}

void FLineRenderData::AddGridLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, float GridSpacing, ELineDepthMode DepthMode) {
    if (WidthPixels <= 0.0f || (End - Start).LengthSquared() <= 0.0f) {
        return;
    }
    mLines.push_back(FLineProbe{Start, End, Color, WidthPixels, GridSpacing, DepthMode});
}

void FLineRenderData::AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels, ELineDepthMode DepthMode) {
    if (Length <= 0.0f || Direction.LengthSquared() <= 0.0f) {
        return;
    }
    FVector3 NormalizedDirection{Direction};
    NormalizedDirection.Normalize();
    AddLine(Origin, Origin + NormalizedDirection * Length, Color, WidthPixels, DepthMode);
}

const TArray<FLineProbe>& FLineRenderData::GetLines() const {
    return mLines;
}

void FLineRenderData::Clear() {
    mLines.clear();
}
