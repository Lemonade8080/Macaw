#include "pch.h"
#include "Core/Property/IPropertyEditorContext.h"
#include "UBillboardTextComponent.h"

#include "Core/Asset/IAssetRegistry.h"
#include "Asset/UFont.h"

#include "Asset/Pipeline/UPipeline.h"

#include "World/AActor.h"
#include "World/UWorld.h"

#include "World/Subsystem/UTextSubsystem.h"

namespace {
    bool DecodeKoreanUTF8(const FString& Text, TArray<char32_t>& OutCodePoints) {
        OutCodePoints.clear();

        if (Text.empty()) {
            return true;
        }

        const int WideLength{MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), static_cast<int>(Text.size()), nullptr, 0)};

        if (WideLength <= 0) {
            return false;
        }

        std::wstring WideText{};
        WideText.resize(WideLength);

        const int ConvertedLength{MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), static_cast<int>(Text.size()), WideText.data(), WideLength)};

        if (ConvertedLength != WideLength) {
            return false;
        }

        for (wchar_t Character : WideText) {
            OutCodePoints.push_back(static_cast<char32_t>(Character));
        }
        return true;
    }
}

void UBillboardTextComponent::SetFontHandle(FAssetHandle InFontHandle) {
    if (mFontHandle == InFontHandle) {
        return;
    }

    mFontHandle = InFontHandle;
    UWorld* World{GetBelongingWorld()};
    const IAssetRegistry* AssetRegistry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mFontAssetPath = AssetRegistry != nullptr && AssetRegistry->GetAssetPath(mFontHandle) != nullptr ? *AssetRegistry->GetAssetPath(mFontHandle) : FAssetPath{};
    mFontAssetGuid = AssetRegistry != nullptr && AssetRegistry->GetAssetGuid(mFontHandle) != nullptr ? *AssetRegistry->GetAssetGuid(mFontHandle) : FGuid{};
    RebuildTextGeometry();
}

void UBillboardTextComponent::SetPipelineHandle(FAssetHandle InPipelineHandle) {
    mPipelineHandle = InPipelineHandle;
    UWorld* World{GetBelongingWorld()};
    const IAssetRegistry* AssetRegistry{World != nullptr ? World->GetAssetRegistry() : nullptr};
    mPipelineAssetPath = AssetRegistry != nullptr && AssetRegistry->GetAssetPath(mPipelineHandle) != nullptr ? *AssetRegistry->GetAssetPath(mPipelineHandle) : FAssetPath{};
    mPipelineAssetGuid = AssetRegistry != nullptr && AssetRegistry->GetAssetGuid(mPipelineHandle) != nullptr ? *AssetRegistry->GetAssetGuid(mPipelineHandle) : FGuid{};
}

void UBillboardTextComponent::SetText(const FString& InText) {
    if (mText == InText) {
        return;
    }

    mText = InText;
    RebuildTextGeometry();
}

void UBillboardTextComponent::SetColor(const FVector4& InColor) {
    mColor = InColor;
}

void UBillboardTextComponent::SetCharacterHeight(float InCharacterHeight) {
    const float NewHeight{std::max(InCharacterHeight, 0.001f)};

    if (mCharacterHeight == NewHeight) {
        return;
    }

    mCharacterHeight = NewHeight;
    RebuildTextGeometry();
}

void UBillboardTextComponent::SetLetterSpacing(float InLetterSpacing) {
    if (mLetterSpacing == InLetterSpacing) {
        return;
    }

    mLetterSpacing = InLetterSpacing;
    RebuildTextGeometry();
}

void UBillboardTextComponent::SetLineSpacing(float InLineSpacing) {
    if (mLineSpacing == InLineSpacing) {
        return;
    }

    mLineSpacing = InLineSpacing;
    RebuildTextGeometry();
}

FAssetHandle UBillboardTextComponent::GetFontHandle() const {
    return mFontHandle;
}

FAssetHandle UBillboardTextComponent::GetPipelineHandle() const {
    return mPipelineHandle;
}

const FString& UBillboardTextComponent::GetText() const {
    return mText;
}

const FVector4& UBillboardTextComponent::GetColor() const {
    return mColor;
}

float UBillboardTextComponent::GetCharacterHeight() const {
    return mCharacterHeight;
}

float UBillboardTextComponent::GetLetterSpacing() const {
    return mLetterSpacing;
}

float UBillboardTextComponent::GetLineSpacing() const {
    return mLineSpacing;
}

const TArray<FTextVertex>& UBillboardTextComponent::GetVertices() const {
    return mVertices;
}

bool UBillboardTextComponent::MakeTextRender(FTextProbe& OutProbe) const {
    if (!IsActive() || !IsVisible() || !mFontHandle || !mPipelineHandle || mVertices.empty()) {
        return false;
    }

    if (!TryGetTextWorld(OutProbe.mWorld)) {
        return false;
    }

    OutProbe.mFontHandle = mFontHandle;
    OutProbe.mPipelineHandle = mPipelineHandle;
    OutProbe.mColor = mColor;
    OutProbe.mVertices = mVertices;

    return true;
}

void UBillboardTextComponent::RebuildTextGeometry() {
    mVertices.clear();

    if (!mFontHandle || mText.empty() || mCharacterHeight <= 0.0f) {
        return;
    }

    AActor* Owner{GetOwner()};

    if (Owner == nullptr || Owner->GetWorld() == nullptr) {
        return;
    }

    const IAssetRegistry* AssetRegistry{Owner->GetWorld()->GetAssetRegistry()};

    if (AssetRegistry == nullptr) {
        return;
    }

    const UFont* Font{AssetRegistry->ResolveAsset<UFont>(mFontHandle)};
    IAssetRegistryMutator* AssetRegistryMutator{Owner->GetWorld()->GetAssetRegistryMutator()};

    if (Font == nullptr || AssetRegistryMutator == nullptr) {
        return;
    }

    const FFontMetrics& Metrics{Font->GetFontMetrics()};

    if (Metrics.mLineHeight <= 0.0f) {
        return;
    }

    TArray<char32_t> CodePoints{};

    if (!DecodeKoreanUTF8(mText, CodePoints)) {
        return;
    }

    const float PixelToWorld{mCharacterHeight / Metrics.mLineHeight};

    float PenX{0.0f};
    float BaselineY{0.0f};

    for (char32_t CodePoint : CodePoints) {
        if (CodePoint == U'\r') {
            continue;
        }

        if (CodePoint == U'\n') {
            PenX = 0.0f;
            BaselineY -= mCharacterHeight + mLineSpacing;

            continue;
        }

        const FFontGlyph* Glyph{AssetRegistryMutator->GetOrCreateFontGlyph(mFontHandle, CodePoint)};

        if (Glyph == nullptr) {
            Glyph = AssetRegistryMutator->GetOrCreateFontGlyph(mFontHandle, U'\uFFFD');
        }

        if (Glyph == nullptr) {
            continue;
        }

        if (Glyph->mBitmapWidth > 0 && Glyph->mBitmapHeight > 0) {
            FTextVertex Vertex{};

            Vertex.mLocalPosition.mX = PenX + static_cast<float>(Glyph->mBearingX) * PixelToWorld;
            Vertex.mLocalPosition.mY = BaselineY + static_cast<float>(Glyph->mBearingY) * PixelToWorld;
            Vertex.mSize.mX = static_cast<float>(Glyph->mBitmapWidth) * PixelToWorld;
            Vertex.mSize.mY = static_cast<float>(Glyph->mBitmapHeight) * PixelToWorld;
            Vertex.mUvMin = Glyph->mUvMin;
            Vertex.mUvMax = Glyph->mUvMax;

            mVertices.push_back(Vertex);
        }
        PenX += Glyph->mAdvanceX * PixelToWorld + mLetterSpacing;
    }
    if (mVertices.empty()) {
        return;
    }

    const FTextVertex& FirstVertex{mVertices.front()};
    float MinLeft{FirstVertex.mLocalPosition.mX};
    float MaxRight{FirstVertex.mLocalPosition.mX + FirstVertex.mSize.mX};
    float MaxTop{FirstVertex.mLocalPosition.mY};
    float MinBottom{FirstVertex.mLocalPosition.mY - FirstVertex.mSize.mY};

    for (const FTextVertex& Vertex : mVertices) {
        MinLeft = std::min(MinLeft, Vertex.mLocalPosition.mX);
        MaxRight = std::max(MaxRight, Vertex.mLocalPosition.mX + Vertex.mSize.mX);
        MaxTop = std::max(MaxTop, Vertex.mLocalPosition.mY);
        MinBottom = std::min(MinBottom, Vertex.mLocalPosition.mY - Vertex.mSize.mY);
    }

    const float CenterX{(MinLeft + MaxRight) * 0.5f};
    const float CenterY{(MinBottom + MaxTop) * 0.5f};

    for (FTextVertex& Vertex : mVertices) {
        Vertex.mLocalPosition.mX -= CenterX;
        Vertex.mLocalPosition.mY -= CenterY;
    }
}

void UBillboardTextComponent::OnRegister() {
    UPrimitiveComponent::OnRegister();

    UWorld* World{GetBelongingWorld()};

    if (World != nullptr) {
        const IAssetRegistry* AssetRegistry{World->GetAssetRegistry()};
        if (AssetRegistry != nullptr) {
            if (AssetRegistry->ResolveAsset<UFont>(mFontHandle) == nullptr) {
                mFontHandle = AssetRegistry->FindAsset(FAssetPath{"/Game/Font/NotoSansKR-Medium.ttf"});
            }

            if (AssetRegistry->ResolveAsset<UPipeline>(mPipelineHandle) == nullptr) {
                mPipelineHandle = AssetRegistry->FindAsset(FAssetPath{"/Game/Pipeline/Text.json"});
            }
        }

        World->GetTextSubsystem().RegisterComponent(this);
    }
    RebuildTextGeometry();
}

void UBillboardTextComponent::OnUnregister() {
    UWorld* World{GetBelongingWorld()};

    if (World != nullptr) {
        World->GetTextSubsystem().UnregisterComponent(this);
    }

    UPrimitiveComponent::OnUnregister();
}

void UBillboardTextComponent::Serialize(FArchive& Archive) {
    UPrimitiveComponent::Serialize(Archive);
    const IAssetRegistry* AssetRegistry{Archive.GetAssetRegistry()};
    if (Archive.IsSaving() && AssetRegistry != nullptr) {
        if (const FAssetPath* AssetPath{AssetRegistry->GetAssetPath(mFontHandle)}) {
            mFontAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{AssetRegistry->GetAssetGuid(mFontHandle)}) {
            mFontAssetGuid = *AssetGuid;
        }
        if (const FAssetPath* AssetPath{AssetRegistry->GetAssetPath(mPipelineHandle)}) {
            mPipelineAssetPath = *AssetPath;
        }
        if (const FGuid* AssetGuid{AssetRegistry->GetAssetGuid(mPipelineHandle)}) {
            mPipelineAssetGuid = *AssetGuid;
        }
    }
    Archive.Serialize("FontAssetGuid", mFontAssetGuid);
    Archive.Serialize("FontAssetPath", mFontAssetPath.mPath);
    Archive.Serialize("PipelineAssetGuid", mPipelineAssetGuid);
    Archive.Serialize("PipelineAssetPath", mPipelineAssetPath.mPath);
    if (Archive.IsLoading()) {
        mFontHandle = AssetRegistry != nullptr ? AssetRegistry->FindAsset(mFontAssetGuid) : FAssetHandle{};
        if (!mFontHandle && AssetRegistry != nullptr) {
            mFontHandle = AssetRegistry->FindAsset(mFontAssetPath);
        }
        mPipelineHandle = AssetRegistry != nullptr ? AssetRegistry->FindAsset(mPipelineAssetGuid) : FAssetHandle{};
        if (!mPipelineHandle && AssetRegistry != nullptr) {
            mPipelineHandle = AssetRegistry->FindAsset(mPipelineAssetPath);
        }
    }
    Archive.Serialize("Text", mText);
    Archive.Serialize("Color", mColor);
    Archive.Serialize("CharacterHeight", mCharacterHeight);
    Archive.Serialize("LetterSpacing", mLetterSpacing);
    Archive.Serialize("LineSpacing", mLineSpacing);
}

bool UBillboardTextComponent::TryGetTextWorld(FMatrix& OutWorld) const {
    OutWorld = GetComponentToWorld();
    return true;
}

void UBillboardTextComponent::DrawPanels(IPropertyEditorContext* Context) {
    UPrimitiveComponent::DrawPanels(Context);

    if (!Context->BeginCategory("Billboard Text")) {
        return;
    }

    Context->DrawText("Text", GetText(), [this](const FString& NewText) {
        SetText(NewText);
    });
    Context->DrawColor("Color", GetColor(), [this](const FVector4& NewColor) {
        SetColor(NewColor);
    });
    Context->DrawFloat("Character Height", GetCharacterHeight(), 0.01f, 0.001f, 1000.0f, [this](float NewHeight) {
        SetCharacterHeight(NewHeight);
    });
    Context->DrawFloat("Letter Spacing", GetLetterSpacing(), 0.01f, -100.0f, 100.0f, [this](float NewSpacing) {
        SetLetterSpacing(NewSpacing);
    });
    Context->DrawFloat("Line Spacing", GetLineSpacing(), 0.01f, -100.0f, 100.0f, [this](float NewSpacing) {
        SetLineSpacing(NewSpacing);
    });

    Context->DrawAssetPicker("Font", *UFont::StaticTypeInfo(), GetFontHandle(), [this](FAssetHandle NewHandle) {
        SetFontHandle(NewHandle);
    });
    Context->DrawAssetPicker("Pipeline", *UPipeline::StaticTypeInfo(), GetPipelineHandle(), [this](FAssetHandle NewHandle) {
        SetPipelineHandle(NewHandle);
    });
}
