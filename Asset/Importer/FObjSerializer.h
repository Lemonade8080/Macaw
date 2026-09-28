#pragma once
#include "Asset/FObjInfo.h"

class FObjSerializer {
public:
    static constexpr Uint32 MagicNumber{0x4d45534d}; // MESM : Macaw Engine Static Mesh
    static constexpr Uint32 CurrentVersion{4};

    static bool SaveBinary(const FGeometry& GeometryData, const FString& FilePath);
    static bool LoadBinary(const FString& FilePath, FGeometry& OutGeoData, Uint32& LoadedVersion);

private:
    static bool WriteFString(std::ofstream& Out, const FString& Str);
    static bool ReadFString(std::ifstream& In, FString& OutStr);
};
