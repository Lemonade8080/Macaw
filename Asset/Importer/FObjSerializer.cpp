#include "pch.h"
#include "FObjSerializer.h"
#include <fstream>

bool FObjSerializer::SaveBinary(const FGeometry& GeometryData, const FString& FilePath) {
    std::ofstream Out{FilePath.c_str(), std::ios::binary};
    if (!Out.is_open()) {
        return false;
    }

    Out.write(reinterpret_cast<const char*>(&MagicNumber), sizeof(MagicNumber));
    Out.write(reinterpret_cast<const char*>(&CurrentVersion), sizeof(CurrentVersion));

    //Position
    Uint32 PositionCount{static_cast<Uint32>(GeometryData.mPositions.size())};
    //배열 개수
    Out.write(reinterpret_cast<const char*>(&PositionCount), sizeof(PositionCount));
    //버텍스 위치값 저장
    Out.write(reinterpret_cast<const char*>(GeometryData.mPositions.data()), PositionCount * sizeof(FVector));

    //Normal
    Uint32 NormalCount{static_cast<Uint32>(GeometryData.mNormals.size())};
    Out.write(reinterpret_cast<const char*>(&NormalCount), sizeof(NormalCount));
    Out.write(reinterpret_cast<const char*>(GeometryData.mNormals.data()), NormalCount * sizeof(FVector));

    //UV
    Uint32 UVCount{static_cast<Uint32>(GeometryData.mTexCoords.size())};
    Out.write(reinterpret_cast<const char*>(&UVCount), sizeof(UVCount));
    Out.write(reinterpret_cast<const char*>(GeometryData.mTexCoords.data()), UVCount * sizeof(FVector2));

    //Index
    Uint32 IndexCount{static_cast<Uint32>(GeometryData.mIndices.size())};
    Out.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
    Out.write(reinterpret_cast<const char*>(GeometryData.mIndices.data()), IndexCount * sizeof(Uint32));

    //Material File Name
    WriteFString(Out, GeometryData.mMaterialFileName);

    //Material Names
    Uint32 MaterialNamesCount{static_cast<Uint32>(GeometryData.mMaterialNames.size())};
    Out.write(reinterpret_cast<const char*>(&MaterialNamesCount), sizeof(MaterialNamesCount));
    for (Uint32 I{0}; I < MaterialNamesCount; I++) {
        WriteFString(Out, GeometryData.mMaterialNames[I]);
    }
    //Out.write(reinterpret_cast<const char*>(GeometryData.MaterialNames.data()), MaterialNamesCount * sizeof(FString));

    //SubMeshIndexCounts
    Uint32 SubMeshIndexCount{static_cast<Uint32>(GeometryData.mSubMeshIndexCounts.size())};
    Out.write(reinterpret_cast<const char*>(&SubMeshIndexCount), sizeof(SubMeshIndexCount));
    Out.write(reinterpret_cast<const char*>(GeometryData.mSubMeshIndexCounts.data()), SubMeshIndexCount * sizeof(Uint32));

    Uint32 ColorCount{static_cast<Uint32>(GeometryData.mColors.size())};
    Out.write(reinterpret_cast<const char*>(&ColorCount), sizeof(ColorCount));
    Out.write(reinterpret_cast<const char*>(GeometryData.mColors.data()), ColorCount * sizeof(FColor4));

    return static_cast<bool>(Out);
}

bool FObjSerializer::LoadBinary(const FString& FilePath, FGeometry& OutGeoData, Uint32& LoadedVersion) {
    LoadedVersion = {};
    std::ifstream In{FilePath.c_str(), std::ios::binary};
    if (!In.is_open()) {
        return false;
    }

    Uint32 Magic{0};
    Uint32 Version{0};

    In.read(reinterpret_cast<char*>(&Magic), sizeof(Magic));
    In.read(reinterpret_cast<char*>(&Version), sizeof(Version));

    //버전이 안 맞으면 바이너리를 읽어오지 않는다.
    if (Magic != MagicNumber || (Version != 3 && Version != CurrentVersion)) {
        return false;
    }
    LoadedVersion = Version;

    Uint32 PositionCount{0};
    In.read(reinterpret_cast<char*>(&PositionCount), sizeof(PositionCount));
    OutGeoData.mPositions.resize(PositionCount);
    In.read(reinterpret_cast<char*>(OutGeoData.mPositions.data()), sizeof(FVector) * PositionCount);

    Uint32 NormalCount{0};
    In.read(reinterpret_cast<char*>(&NormalCount), sizeof(NormalCount));
    OutGeoData.mNormals.resize(NormalCount);
    In.read(reinterpret_cast<char*>(OutGeoData.mNormals.data()), sizeof(FVector) * NormalCount);

    Uint32 UVCount{0};
    In.read(reinterpret_cast<char*>(&UVCount), sizeof(UVCount));
    OutGeoData.mTexCoords.resize(UVCount);
    In.read(reinterpret_cast<char*>(OutGeoData.mTexCoords.data()), sizeof(FVector2) * UVCount);

    Uint32 IndexCount{0};
    In.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
    OutGeoData.mIndices.resize(IndexCount);
    In.read(reinterpret_cast<char*>(OutGeoData.mIndices.data()), sizeof(Uint32) * IndexCount);

    //Material File Name
    ReadFString(In, OutGeoData.mMaterialFileName);

    //Material Names
    Uint32 MaterialNamesCount{0};
    In.read(reinterpret_cast<char*>(&MaterialNamesCount), sizeof(MaterialNamesCount));
    OutGeoData.mMaterialNames.resize(MaterialNamesCount);
    for (Uint32 I{0}; I < MaterialNamesCount; I++) {
        ReadFString(In, OutGeoData.mMaterialNames[I]);
    }
    //In.read(reinterpret_cast<char*>(OutGeoData.MaterialNames.data()), sizeof(FString) * MaterialNamesCount);;

    //SubMeshIndexCounts
    Uint32 SubMeshIndexCount{0};
    In.read(reinterpret_cast<char*>(&SubMeshIndexCount), sizeof(SubMeshIndexCount));
    OutGeoData.mSubMeshIndexCounts.resize(SubMeshIndexCount);
    In.read(reinterpret_cast<char*>(OutGeoData.mSubMeshIndexCounts.data()), sizeof(Uint32) * SubMeshIndexCount);

    Uint32 ColorCount{0};
    In.read(reinterpret_cast<char*>(&ColorCount), sizeof(ColorCount));
    OutGeoData.mColors.resize(ColorCount);
    In.read(reinterpret_cast<char*>(OutGeoData.mColors.data()), sizeof(FColor4) * ColorCount);

    return static_cast<bool>(In);
}

bool FObjSerializer::WriteFString(std::ofstream& Out, const FString& Str) {
    Uint32 Len{static_cast<Uint32>(Str.size())};
    //크기
    Out.write(reinterpret_cast<const char*>(&Len), sizeof(Len));
    //데이터
    Out.write(Str.data(), Len);
    return static_cast<bool>(Out);
}

bool FObjSerializer::ReadFString(std::ifstream& In, FString& OutStr) {
    Uint32 Len{0};
    In.read(reinterpret_cast<char*>(&Len), sizeof(Len));
    if (!In)
        return false;

    OutStr.resize(Len);
    In.read(OutStr.data(), Len);
    return static_cast<bool>(In);
}
