#pragma once
#include <d3d11.h>

#include "FObjInfo.h"
#include "Asset/FAssetRegistry.h"
#include "Asset/UMesh.h"

////OBJ의 v/vt/vn 인덱스 조합 하나 = GPU 정점 하나. 같은 조합이 또 나오면 새 정점을 만들지 않고 재사용한다.
//struct FFaceVertexKey
//{
//    Int32 PositionIndex;
//    Int32 UVIndex;
//    Int32 NormalIndex;
//
//    bool operator==(const FFaceVertexKey& Other) const noexcept
//    {
//        return PositionIndex == Other.PositionIndex
//            && UVIndex == Other.UVIndex
//            && NormalIndex == Other.NormalIndex;
//    }
//};

//
//struct FFaceVertexKeyHash
//{
//    size_t operator()(const FFaceVertexKey& Key) const noexcept
//    {
//        size_t Hash = std::hash<Int32>{}(Key.PositionIndex);
//        Hash = Hash * 31 + std::hash<Int32>{}(Key.UVIndex);
//        Hash = Hash * 31 + std::hash<Int32>{}(Key.NormalIndex);
//        return Hash;
//    }
//};
//
//struct FObjInfo
//{
//    FString AssetName;
//    //이 오브젝트의 머티리얼 정보가 담긴 mtl 파일 이름
//    FString MaterialFileName;
//
//    TArray<FVector>  Positions;
//    TArray<FVector2> UVs;
//    TArray<FVector>  Normals;
//
//    // face 하나를 구성하는 정점 순서대로 : 나중에 인덱스 버퍼의 순서가 된다.
//    // v/vt/vn 조합
//    //다각형에 대응하기 위해서 1개 면이 저장하는 버텍스들을 TArray로 한번 더 감쌉니다.
//    TArray<TArray<FFaceVertex>> FaceVertices_Polygon;
//
//    //머티리얼 이름들
//    //SubMesh와 인덱스 매칭한다.
//    TArray<FString> MaterialNames;
//
//    //동일한 머티리얼을 쓰는 정점들의 개수
//    //MaterialNames와 인덱스 매칭한다.
//    //MaterialNames[0]를 쓰는 정점의 개수는 SubMesh[0]개
//    TArray<Int32> SubMesh;
//};
//
//struct FGeometry
//{
//    TArray<FVector>  Positions;
//    TArray<FVector>  Normals;
//    TArray<FVector2> TexCoords;
//    TArray<Uint32>    Indices;
//
//    FString MaterialFileName;
//    TArray<FString> MaterialNames;
//    TArray<Uint32> SubMeshIndexCounts;
//};
class FObjImporter {
public:
    FObjImporter() = default;
    ~FObjImporter() = default;

    FObjImporter(const FObjImporter&) = delete;
    FObjImporter& operator=(const FObjImporter&) = delete;

    FObjImporter(FObjImporter&&) noexcept = default;
    FObjImporter& operator=(FObjImporter&&) noexcept = default;

    //Obj 파일 로드
    //std::unique_ptr<UObject> LoadObjFile(const FString& FilePath);
    bool LoadObjFile(const FString& FilePath, FGeometry& OutGeometry);
    //한줄 나누기
    static TArray<FString> SplitTokens(const FString& Line);

    //obj의 f값을 인덱스로 바꾸기 위해 1을 뺍니다.
    //음수값이라면 뒤에서부터 가져옵니다.
    Int32 NormalizeIndex(Int32 RawIndex, Int32 ArraySize) const;

private:
    //파싱된 ObjInfo로 Vertex Position, Index, Normal, TexCoord 배열을 만든다.
    bool BuildGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const;

    //다각형이라면 계산을 다르게 처리
    bool BuildPolygonGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const;

    //1개 버텍스 데이터 저장
    void AddPNTIArray(const FFaceVertex& TargetVertex, const FObjInfo& ObjInfo, FGeometry& OutGeometry, std::unordered_map<FFaceVertexKey, Uint32, FFaceVertexKeyHash>& CacheMap) const;

    //정점들의 Normal 평균 구하기
    FVector ComputeFaceNormal(const TArray<FFaceVertex>& PolygonVertices, const TArray<FVector>& Positions, Int32 PositionCount) const;

private:
    FString mLastError{};
    FVector mPositionCoordTransX{FVector{1.f, 0.f, 0.f}};
    FVector mPositionCoordTransY{FVector{0.f, 1.f, 0.f}};
    FVector mPositionCoordTransZ{FVector{0.f, 0.f, 1.f}};
};
