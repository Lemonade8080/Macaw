#pragma once

#include "Core/STL.h"
#include "ILineDrawContext.h"

struct FLineProbe {
    FVector3 mStart{};
    FVector3 mEnd{};
    FVector4 mColor{};
    float mWidthPixels{1.0f};
    float mGridSpacing{};
    ELineDepthMode mDepthMode{ELineDepthMode::DepthTested};
};

class FLineRenderData final : public ILineDrawContext {
public:
    void AddLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested) override;
    void AddGridLine(const FVector3& Start, const FVector3& End, const FVector4& Color, float WidthPixels, float GridSpacing, ELineDepthMode DepthMode);
    void AddRay(const FVector3& Origin, const FVector3& Direction, float Length, const FVector4& Color, float WidthPixels = 1.0f, ELineDepthMode DepthMode = ELineDepthMode::DepthTested);

    const TArray<FLineProbe>& GetLines() const;
    void Clear();

private:
    TArray<FLineProbe> mLines{};
};
