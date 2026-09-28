#include "pch.h"
#include "USurfaceOpaque.h"

#include <cstring>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace {
    struct FSurfaceOpaqueGroupGPUData {
        FVector4 mDiffuseAndOpacity{1.0f, 1.0f, 1.0f, 1.0f};
        FVector4 mAmbientAndShininess{};
        FVector4 mSpecularAndRefractionIndex{};
        FVector4 mEmissiveAndSharpness{};
        FVector4 mTransmissionFilter{};
        Int32 mIlluminationModel{};
        Uint32 mDissolveHalo{};
        float mPadding0{};
        float mPadding1{};
        FVector4 mReserved1{};
        FVector4 mReserved2{};
    };

    static_assert(sizeof(FSurfaceOpaqueGroupGPUData) == MaterialGpuStride);
    static_assert(offsetof(FSurfaceOpaqueGroupGPUData, mIlluminationModel) == 80);
    static_assert(offsetof(FSurfaceOpaqueGroupGPUData, mDissolveHalo) == 84);

    bool ParseVector3(std::istringstream& Stream, FVector3& OutValue) {
        float X{0.0f};
        float Y{0.0f};
        float Z{0.0f};

        if (!(Stream >> X >> Y >> Z)) {
            return false;
        }

        OutValue = FVector3{X, Y, Z};
        return true;
    }

    std::filesystem::path GetTextureReference(std::istringstream& Stream) {
        std::string Token{};
        std::string TextureReference{};

        while (Stream >> Token) {
            TextureReference = Token;
        }

        return TextureReference;
    }

    bool LoadTextureMap(FMaterialTextureMap& OutTextureMap, std::istringstream& Stream, const std::filesystem::path& MtlPath, const USurfaceOpaque::FTextureResolver& TextureResolver) {
        const std::filesystem::path TextureReference{GetTextureReference(Stream)};

        if (TextureReference.empty()) {
            return false;
        }

        const std::filesystem::path TexturePath{(MtlPath.parent_path() / TextureReference).lexically_normal()};
        OutTextureMap.mSourcePath = TextureReference.generic_string().c_str();
        OutTextureMap.mTexture = TextureResolver(TexturePath);
        return static_cast<bool>(OutTextureMap.mTexture);
    }
}

void USurfaceOpaque::Reset() {
    mGroups.clear();
    MarkGPUDataDirty();
}

bool USurfaceOpaque::Initialize(ID3D11Device* Device, const std::filesystem::path& MtlPath, const FTextureResolver& TextureResolver) {
    if (!TextureResolver || !UAsset::Initialize(Device, MtlPath)) {
        return false;
    }

    Reset();

    std::ifstream File{MtlPath};

    if (!File.is_open()) {
        return false;
    }

    FMaterialGroup CurrentGroup{};
    bool HasCurrentGroup{false};
    std::string RawLine{};

    while (std::getline(File, RawLine)) {
        if (!RawLine.empty() && RawLine.back() == '\r') {
            RawLine.pop_back();
        }

        std::istringstream Stream{RawLine};
        std::string Command{};
        Stream >> Command;

        if (Command.empty() || Command[0] == '#') {
            continue;
        }

        if (Command == "newmtl") {
            std::string Name{};
            Stream >> Name;

            if (Name.empty()) {
                continue;
            }

            if (HasCurrentGroup) {
                mGroups.push_back(std::move(CurrentGroup));
            }

            CurrentGroup = {};
            CurrentGroup.mName = Name.c_str();
            HasCurrentGroup = true;
            continue;
        }

        if (!HasCurrentGroup) {
            continue;
        }

        if (Command == "Ka") {
            ParseVector3(Stream, CurrentGroup.mAmbient);
        } else if (Command == "Kd") {
            ParseVector3(Stream, CurrentGroup.mDiffuse);
        } else if (Command == "Ks") {
            ParseVector3(Stream, CurrentGroup.mSpecular);
        } else if (Command == "Ke") {
            ParseVector3(Stream, CurrentGroup.mEmissive);
        } else if (Command == "Tf") {
            ParseVector3(Stream, CurrentGroup.mTransmissionFilter);
        } else if (Command == "Ns") {
            Stream >> CurrentGroup.mShininess;
        } else if (Command == "Ni") {
            Stream >> CurrentGroup.mRefractionIndex;
        } else if (Command == "d") {
            std::string Token{};
            Stream >> Token;

            if (Token == "-halo") {
                CurrentGroup.mBDissolveHalo = true;
                Stream >> CurrentGroup.mOpacity;
            } else if (!Token.empty()) {
                std::istringstream OpacityStream{Token};
                OpacityStream >> CurrentGroup.mOpacity;
            }
        } else if (Command == "Tr") {
            float Transparency{0.0f};

            if (Stream >> Transparency) {
                CurrentGroup.mOpacity = 1.0f - Transparency;
            }
        } else if (Command == "illum") {
            Stream >> CurrentGroup.mIlluminationModel;
        } else if (Command == "sharpness") {
            Stream >> CurrentGroup.mSharpness;
        } else if (Command == "map_Ka") {
            LoadTextureMap(CurrentGroup.mAmbientTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_Kd") {
            LoadTextureMap(CurrentGroup.mDiffuseTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_Ks") {
            LoadTextureMap(CurrentGroup.mSpecularTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_Ke") {
            LoadTextureMap(CurrentGroup.mEmissiveTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_Tf") {
            LoadTextureMap(CurrentGroup.mTransmissionTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_Ns") {
            LoadTextureMap(CurrentGroup.mShininessTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_d") {
            LoadTextureMap(CurrentGroup.mOpacityTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "map_Bump" || Command == "map_bump" || Command == "bump") {
            LoadTextureMap(CurrentGroup.mBumpTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "norm") {
            LoadTextureMap(CurrentGroup.mNormalTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "disp") {
            LoadTextureMap(CurrentGroup.mDisplacementTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "decal") {
            LoadTextureMap(CurrentGroup.mDecalTexture, Stream, MtlPath, TextureResolver);
        } else if (Command == "refl") {
            LoadTextureMap(CurrentGroup.mReflectionTexture, Stream, MtlPath, TextureResolver);
        }
    }

    if (HasCurrentGroup) {
        mGroups.push_back(std::move(CurrentGroup));
    }

    if (mGroups.empty()) {
        return false;
    }

    MarkGPUDataDirty();
    return true;
}

void USurfaceOpaque::BuildGPUData(FMaterialGPUSlot& OutSlot) const {
    BuildGPUData(0, OutSlot);
}

void USurfaceOpaque::BuildGPUData(Uint32 GroupIndex, FMaterialGPUSlot& OutSlot) const {
    if (GroupIndex >= mGroups.size()) {
        OutSlot = {};
        return;
    }

    const FMaterialGroup& Group{mGroups[GroupIndex]};
    FSurfaceOpaqueGroupGPUData Data{};
    Data.mDiffuseAndOpacity = FVector4{Group.mDiffuse.mX, Group.mDiffuse.mY, Group.mDiffuse.mZ, Group.mOpacity};
    Data.mAmbientAndShininess = FVector4{Group.mAmbient.mX, Group.mAmbient.mY, Group.mAmbient.mZ, Group.mShininess};
    Data.mSpecularAndRefractionIndex = FVector4{Group.mSpecular.mX, Group.mSpecular.mY, Group.mSpecular.mZ, Group.mRefractionIndex};
    Data.mEmissiveAndSharpness = FVector4{Group.mEmissive.mX, Group.mEmissive.mY, Group.mEmissive.mZ, Group.mSharpness};
    Data.mTransmissionFilter = FVector4{Group.mTransmissionFilter.mX, Group.mTransmissionFilter.mY, Group.mTransmissionFilter.mZ, 0.0f};
    Data.mIlluminationModel = Group.mIlluminationModel;
    Data.mDissolveHalo = Group.mBDissolveHalo ? 1u : 0u;

    std::memcpy(OutSlot.mData.data(), &Data, sizeof(Data));
}

FMaterialChunkSignature USurfaceOpaque::BuildChunkSignature() const {
    return BuildChunkSignature(0);
}

FMaterialChunkSignature USurfaceOpaque::BuildChunkSignature(Uint32 GroupIndex) const {
    FMaterialChunkSignatureBuilder Builder{};

    if (GroupIndex >= mGroups.size()) {
        return Builder.Build();
    }

    const FMaterialGroup& Group{mGroups[GroupIndex]};
    Builder.AddTexture(Group.mAmbientTexture.mTexture);
    Builder.AddTexture(Group.mDiffuseTexture.mTexture);
    Builder.AddTexture(Group.mSpecularTexture.mTexture);
    Builder.AddTexture(Group.mEmissiveTexture.mTexture);
    Builder.AddTexture(Group.mTransmissionTexture.mTexture);
    Builder.AddTexture(Group.mShininessTexture.mTexture);
    Builder.AddTexture(Group.mOpacityTexture.mTexture);
    Builder.AddTexture(Group.mBumpTexture.mTexture);
    Builder.AddTexture(Group.mNormalTexture.mTexture);
    Builder.AddTexture(Group.mDisplacementTexture.mTexture);
    Builder.AddTexture(Group.mDecalTexture.mTexture);
    Builder.AddTexture(Group.mReflectionTexture.mTexture);

    return Builder.Build();
}

std::optional<Uint32> USurfaceOpaque::FindGroupIndex(const FString& Name) const {
    for (Uint32 Index{}; Index < mGroups.size(); ++Index) {
        if (mGroups[Index].mName == Name) {
            return Index;
        }
    }

    return std::nullopt;
}

const TArray<FMaterialGroup>& USurfaceOpaque::GetGroups() const {
    return mGroups;
}

bool USurfaceOpaque::ModifyGroup(Uint32 GroupIndex, const std::function<void(FMaterialGroup&)>& Modifier) {
    if (GroupIndex >= mGroups.size() || !Modifier) {
        return false;
    }

    Modifier(mGroups[GroupIndex]);
    MarkGPUDataDirty();
    return true;
}

void USurfaceOpaque::Serialize(FArchive& Ar) {
    UMaterial::Serialize(Ar);
}

Uint32 USurfaceOpaque::GetGPUDataCount() const {
    return mGroups.empty() ? 1u : static_cast<Uint32>(mGroups.size());
}
