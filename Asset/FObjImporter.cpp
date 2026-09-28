#include "pch.h"
#include "FObjImporter.h"
#include <filesystem>
#include <fstream>
#include <format>
#include <unordered_map>

#include "Core/Console/Console.h"

bool FObjImporter::LoadObjFile(const FString& FilePath, FGeometry& OutGeometry) {
    std::ifstream File{FilePath.c_str()};
    if (!File.is_open()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[ObjImporter] Obj Load Failed. Can not Open File");
        return false;
    }

    std::string RawLine{};

    FObjInfo ObjInfo{};

    //SubMesh에 넣기 전 임시 저장 변수
    Int32 TempFaceCount{0};

    //삼각형으로만 되어 있는지, 다각형도 있는지 검사
    bool Ispolygon{false};

    while (std::getline(File, RawLine)) {
        if (!RawLine.empty() && RawLine.back() == '\r') {
            RawLine.pop_back();
        }

        //한줄 씩
        FString Line{RawLine.c_str()};
        TArray<FString> Tokens{SplitTokens(Line)};

        if (Tokens.empty()) {
            continue;
        }

        const FString& Tag{Tokens[0]};

        if (Tag == "v") // v x y z
        {
            //color 값이 있다면 v x y z r g b 와 같은 형태로 들어온다.
            if (Tokens.size() < 4)
                continue;

            FVector Pos{FVector{std::stof(Tokens[1].c_str()), std::stof(Tokens[2].c_str()), std::stof(Tokens[3].c_str())}};

            //각 x,y,z에 좌표계 변환
            Pos = FVector{Pos.Dot(mPositionCoordTransX), Pos.Dot(mPositionCoordTransY), Pos.Dot(mPositionCoordTransZ)};
            ObjInfo.mPositions.push_back(Pos);

            FColor4 Color{FColor4(1.f, 1.f, 1.f, 1.f)};

            //컬러 값이 존재한다면
            if (Tokens.size() > 4) {
                Color.mX = std::stof(Tokens[4].c_str());
                Color.mY = std::stof(Tokens[5].c_str());
                Color.mZ = std::stof(Tokens[6].c_str());
                Color.mW = Tokens.size() > 7 ? std::stof(Tokens[7].c_str()) : 1.f;
            }

            ObjInfo.mColors.push_back(Color);
        } else if (Tag == "vn") // vn x y z
        {
            if (Tokens.size() != 4)
                continue;
            FVector Normal{FVector{std::stof(Tokens[1].c_str()), std::stof(Tokens[2].c_str()), std::stof(Tokens[3].c_str())}};
            Normal = FVector{Normal.Dot(mPositionCoordTransX), Normal.Dot(mPositionCoordTransY), Normal.Dot(mPositionCoordTransZ)};
            ObjInfo.mNormals.push_back(Normal);
        } else if (Tag == "vt") // vt u v
        {
            if (Tokens.size() != 3)
                continue;
            FVector2 UV{std::stof(Tokens[1].c_str()), std::stof(Tokens[2].c_str())};
            ObjInfo.mUVs.push_back(UV);
        } else if (Tag == "o") // o Name
        {
            if (Tokens.size() != 2)
                continue;

            //이름 저장
            ObjInfo.mAssetName = Tokens[1];
        } else if (Tag == "s") {
            //스무딩 그룹이라는데 일단 대기
        } else if (Tag == "usemtl") // usemtl Name
        {
            if (Tokens.size() != 2)
                continue;

            //머티리얼 이름 넣기
            ObjInfo.mMaterialNames.push_back(Tokens[1]);

            //처음엔 FaceViertices가 없어서 넣으면 안된다.
            if (ObjInfo.mFaceVerticesPolygon.size() != 0) {
                //face들을 순회하다가 새로운 머티리얼을 만난다면 지금까지의 face들의 개수를 SubMesh에 넣어준다.
                ObjInfo.mSubMesh.push_back(TempFaceCount);
                TempFaceCount = 0;
            }
        } else if (Tag == "f") {
            //v, v/vt, v//vn, v/vt/vn 네가지 형태 있음.
            if (Tokens.size() == 0)
                continue;

            // f  v  v  v  : 삼각형이면 개수가 4개
            // f  v  v  v  v : 다각형이면 개수가 5개 이상
            if (Tokens.size() > 4)
                Ispolygon = true;

            //임시 저장
            TArray<FFaceVertex> Vertices{};

            //가장 앞인 f를 제외한 나머지 버텍스에 대해서 순회하며 저장
            for (int I{1}; I < Tokens.size(); I++) {
                FString VertexData{Tokens[I]};
                std::size_t FirstIndex{VertexData.find('/')};
                std::size_t SecondIndex{0};
                FFaceVertex Face{};

                // '/'를 찾지 못했다면 vertex position만 있는 것.
                if (FirstIndex == FString::npos) {
                    Face.mPositionIndex = std::stoi(VertexData.c_str());
                } else {
                    // v는 '/'의 위치까지 잘라낸 값
                    //std.substr(pos, count) : pos부터 count개 문자열 반환. count 기본값은 npos로 끝까지 추출
                    Face.mPositionIndex = std::stoi(VertexData.substr(0, FirstIndex).c_str());

                    //위에서 찾은 곳 다음 칸부터 '/'를 또 찾는다.
                    SecondIndex = VertexData.find('/', FirstIndex + 1);
                    if (SecondIndex == FString::npos) {
                        //두번째에 '/'가 없다면 vt만 있다. v/vt
                        //처음 찾은 PositionIndex 다음부터 끝까지(기본값 npos) 잘라내면 vt다
                        Face.mUvIndex = std::stoi(VertexData.substr(FirstIndex + 1).c_str());
                    } else {
                        // v//vn 아니면 v/vt/vn 이다
                        // v/vt/n 인 경우만 vt를 계산하도록 한다.
                        if (SecondIndex > FirstIndex + 1) {
                            // vt는 PositonIndex 다음부터 SecondIndex - positonindex - 1개를 잘라낸다
                            Face.mUvIndex = std::stoi(VertexData.substr(FirstIndex + 1, SecondIndex - FirstIndex - 1).c_str());
                        }

                        //두번째 다음부터 끝까지 잘라내면 n이다
                        Face.mNormalIndex = std::stoi(VertexData.substr(SecondIndex + 1).c_str());
                    }
                }
                Vertices.push_back(Face);
            }

            //SubMesh에 넣어주기 위한 FaceCount값
            TempFaceCount += static_cast<Int32>(Vertices.size());

            //반대로 뒤집기
            std::reverse(Vertices.begin(), Vertices.end());

            ObjInfo.mFaceVerticesPolygon.push_back(Vertices);
        } else if (Tag == "mtllib") {
            ObjInfo.mMaterialFileName = Tokens[1];
        }
    }

    //가장 마지막 Face들의 개수를 SubMesh에 넣어준다.
    ObjInfo.mSubMesh.push_back(TempFaceCount);
    TempFaceCount = 0;

    if (Ispolygon) {
        BuildPolygonGeometry(ObjInfo, OutGeometry);
    } else {
        BuildGeometry(ObjInfo, OutGeometry);
    }

    //다각형이 포함된 모델이라면 배열 조합을 다르게 처리한다.
    return true;
}

bool FObjImporter::BuildGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const {
    if (ObjInfo.mPositions.empty() || ObjInfo.mFaceVerticesPolygon.empty()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. No geometry data to build.");
        return false;
    }

    //초기화
    OutGeometry.mPositions.clear();
    OutGeometry.mNormals.clear();
    OutGeometry.mTexCoords.clear();
    OutGeometry.mIndices.clear();
    OutGeometry.mColors.clear();
    OutGeometry.mMaterialFileName = ObjInfo.mMaterialFileName;
    OutGeometry.mMaterialNames = ObjInfo.mMaterialNames;
    OutGeometry.mSubMeshIndexCounts.clear();
    OutGeometry.mSubMeshIndexCounts.reserve(ObjInfo.mSubMesh.size());

    for (const Int32 IndexCount : ObjInfo.mSubMesh) {
        if (IndexCount < 0) {
            return false;
        }

        OutGeometry.mSubMeshIndexCounts.push_back(static_cast<Uint32>(IndexCount));
    }

    /*const Int32 PositionCount = static_cast<Int32>(ObjInfo.Positions.size());
	const Int32 UVCount = static_cast<Int32>(ObjInfo.UVs.size());
	const Int32 NormalCount = static_cast<Int32>(ObjInfo.Normals.size());*/
    //(PositionIndex, UVIndex, NormalIndex) 조합 -> 이미 만들어둔 OutGeometry 상의 정점 인덱스
    std::unordered_map<FFaceVertexKey, Uint32, FFaceVertexKeyHash> VertexCache{};
    VertexCache.reserve(ObjInfo.mPositions.size());

    OutGeometry.mPositions.reserve(ObjInfo.mPositions.size());
    OutGeometry.mNormals.reserve(ObjInfo.mPositions.size());
    OutGeometry.mTexCoords.reserve(ObjInfo.mPositions.size());
    OutGeometry.mIndices.reserve(ObjInfo.mFaceVerticesPolygon.size());
    OutGeometry.mColors.reserve(ObjInfo.mPositions.size());

    //모든 Face Vertex 들
    for (auto& FaceVertics : ObjInfo.mFaceVerticesPolygon) {
        // 1개 면의 모음
        for (const FFaceVertex& Face : FaceVertics) {
            //기본값(-1)이면 파싱이 깨진 코너이므로 건너뛴다.
            if (Face.mPositionIndex == -1) {
                continue;
            }

            AddPNTIArray(Face, ObjInfo, OutGeometry, VertexCache);
        }
    }

    if (OutGeometry.mPositions.empty() || OutGeometry.mIndices.empty()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. Resulting geometry is empty.");
        return false;
    }

    return true;
}

bool FObjImporter::BuildPolygonGeometry(const FObjInfo& ObjInfo, FGeometry& OutGeometry) const {
    //다각형이라면 Ear Clipping 방식을 따라갑니다.
    //아래 조건을 만족하는 삼각형을 찾아갑니다.
    //ObjInfo.FaceVertices를 순회하며 순서대로 Prev, Current, Next의 정점 3개로 삼각형을 구성합니다.
    //Current는 볼록해야합니다. 오목하다면 다음 삼각형으로 넘어갑니다.
    //		Current - Prev 벡터, Next - Current 벡터를 외적하여 노멀 벡터를 구하고, Face의 노멀과 내적하여 방향이 같다면 볼록, 방향이 다르다면 오목
    //Current가 볼록하다면 3개 정점으로 이루어진 삼각형 안에 다른 정점이 없어야 합니다. 다른 정점이 있다면 넘어갑니다.
    //		다른 모든 정점을 순회하며 검사합니다. 다른 정점 V에 대해 C - P, N - C, P - N 벡터와 V - P, V - C, V - N 벡터와 외적하여 모두가 양수라면 내부에 존재합니다.
    //		즉, 하나라도 양수가 아니라면 외부에 존재하니 성립합니다.
    //위 두가지 조건을 만족하는 삼각형이 나온다면 캐시 버텍스 검사를 통해 없는 버텍스라면 버텍스 위치 배열, 노멀 배열, uv 배열에 같은 인덱스로 해당하는 값들을 추가하고
    // 인덱스 배열에 추가합니다. 있는 버텍스라면 해당 인덱스를 인덱스 버퍼에만 추가합니다.
    //Current 정점을 제외하고 남은 정점에 대하여 해당 과정을 반복합니다.
    //남은 정점이 3개만 남는다면 인덱스 버퍼를 채우고 종료합니다.
    if (ObjInfo.mPositions.empty() || ObjInfo.mFaceVerticesPolygon.empty()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. No geometry data to build.");
        return false;
    }

    //초기화
    OutGeometry.mPositions.clear();
    OutGeometry.mNormals.clear();
    OutGeometry.mTexCoords.clear();
    OutGeometry.mIndices.clear();
    OutGeometry.mColors.clear();
    OutGeometry.mMaterialFileName = ObjInfo.mMaterialFileName;
    OutGeometry.mMaterialNames = ObjInfo.mMaterialNames;
    OutGeometry.mSubMeshIndexCounts.assign(ObjInfo.mSubMesh.size(), 0);

    if (ObjInfo.mSubMesh.empty()) {
        return false;
    }

    const Int32 PositionCount{static_cast<Int32>(ObjInfo.mPositions.size())};
    const Int32 UVCount{static_cast<Int32>(ObjInfo.mUVs.size())};
    const Int32 NormalCount{static_cast<Int32>(ObjInfo.mNormals.size())};

    //(PositionIndex, UVIndex, NormalIndex) 조합 -> 이미 만들어둔 OutGeometry 상의 정점 인덱스
    std::unordered_map<FFaceVertexKey, Uint32, FFaceVertexKeyHash> VertexCache{};
    VertexCache.reserve(ObjInfo.mPositions.size());

    OutGeometry.mPositions.reserve(ObjInfo.mPositions.size());
    OutGeometry.mNormals.reserve(ObjInfo.mPositions.size());
    OutGeometry.mTexCoords.reserve(ObjInfo.mPositions.size());
    OutGeometry.mIndices.reserve(ObjInfo.mFaceVerticesPolygon.size());
    OutGeometry.mColors.reserve(ObjInfo.mPositions.size());

    FFaceVertex Prev{};
    FFaceVertex Current{};
    FFaceVertex Next{};
    FFaceVertex OtherVertex{};
    FVector FaceNormal{};

    //Face의 버텍스들을 순회할 인덱스
    Int32 FaceVertexIndex{0};
    std::size_t CurrentSubMeshIndex{0};
    Int32 CurrentSubMeshSourceIndexCount{0};

    const TArray<FVector>& FacePositions{ObjInfo.mPositions};
    const TArray<FVector2>& FaceUVs{ObjInfo.mUVs};
    const TArray<FVector>& FaceNormals{ObjInfo.mNormals};

    constexpr float ConvexityEpsilon{1e-5f};

    //모든 Face Vertex 들
    //복사본으로 순회한다.
    for (auto FaceVertics : ObjInfo.mFaceVerticesPolygon) {
        const Int32 FaceSourceIndexCount{static_cast<Int32>(FaceVertics.size())};

        if (CurrentSubMeshIndex >= ObjInfo.mSubMesh.size() ||
            FaceSourceIndexCount > ObjInfo.mSubMesh[CurrentSubMeshIndex] - CurrentSubMeshSourceIndexCount) {
            return false;
        }

        const std::size_t FirstGeneratedIndex{OutGeometry.mIndices.size()};
        FaceVertexIndex = 0;

        Int32 FaceVerticesCount{static_cast<Int32>(FaceVertics.size())};
        Int32 LoopCount{0};

        //평면에 수직인 노멀 구하기
        FaceNormal = ComputeFaceNormal(FaceVertics, FacePositions, PositionCount);

        // 1개 면의 모음
        //다각형 가운데 삼각형이 있다면 size가 3이므로 while문 종료
        while (FaceVertics.size() > 3 && FaceVertexIndex < FaceVertics.size()) {
            LoopCount++;
            if (LoopCount > FaceVerticesCount * FaceVerticesCount) {
                break;
            }

            //인덱스
            Int32 PrevIndex{static_cast<Int32>(FaceVertexIndex % FaceVertics.size())};
            Int32 CurrentIndex{static_cast<Int32>((FaceVertexIndex + 1) % FaceVertics.size())};
            Int32 NextIndex{static_cast<Int32>((FaceVertexIndex + 2) % FaceVertics.size())};

            //순서대로 버텍스 할당
            Prev = FaceVertics[PrevIndex];
            Current = FaceVertics[CurrentIndex];
            Next = FaceVertics[NextIndex];

            //포지션 배열 상에서 인덱스
            Int32 PrevPositionIndex{NormalizeIndex(Prev.mPositionIndex, PositionCount)};
            Int32 CurrentPositionIndex{NormalizeIndex(Current.mPositionIndex, PositionCount)};
            Int32 NextPositionIndex{NormalizeIndex(Next.mPositionIndex, PositionCount)};

            FVector NormalPrev{FaceNormals[NormalizeIndex(Prev.mNormalIndex, NormalCount)]};
            FVector NormalCurrent{FaceNormals[NormalizeIndex(Current.mNormalIndex, NormalCount)]};
            FVector NormalNext{FaceNormals[NormalizeIndex(Next.mNormalIndex, NormalCount)]};

            //FaceNormal = (Normal_Prev + Normal_Current + Normal_Next) / 3.f;
            //Current가 볼록한지 오목한지 검사
            //Current - Prev 벡터, Next - Current 벡터를 외적하여 노멀 벡터를 구하고, Face의 노멀과 내적하여 방향이 같다면 볼록, 방향이 다르다면 오목
            FVector Vector1{FacePositions[CurrentPositionIndex] - FacePositions[PrevPositionIndex]};
            FVector Vector2{FacePositions[NextPositionIndex] - FacePositions[CurrentPositionIndex]};

            Vector1.Normalize();
            Vector2.Normalize();

            FVector CrossVector{Vector1.Cross(Vector2)};
            CrossVector.Normalize();

            //Vector_1 = Vector_1.Cross(Vector_2);
            float DotResult{CrossVector.Dot(FaceNormal)};

            //내적값이 음수라면 오목
            if (DotResult < -ConvexityEpsilon) {
                FaceVertexIndex = (FaceVertexIndex + 1) % FaceVertics.size();
                Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[Load OBJ File] Has Concave Vertex");
                continue;
            }

            bool OtherVertexCheck{false};

            for (int I{0}; I < FaceVertics.size(); I++) {
                //자기 자신은 제외
                if (PrevIndex == I || CurrentIndex == I || NextIndex == I) {
                    continue;
                }

                OtherVertex = FaceVertics[I];

                FVector CPVector{FacePositions[CurrentPositionIndex] - FacePositions[PrevPositionIndex]};
                FVector NCVector{FacePositions[NextPositionIndex] - FacePositions[CurrentPositionIndex]};
                FVector PNVector{FacePositions[PrevPositionIndex] - FacePositions[NextPositionIndex]};

                FVector OPVector{FacePositions[NormalizeIndex(OtherVertex.mPositionIndex, PositionCount)] - FacePositions[PrevPositionIndex]};
                FVector OCVector{FacePositions[NormalizeIndex(OtherVertex.mPositionIndex, PositionCount)] - FacePositions[CurrentPositionIndex]};
                FVector ONVector{FacePositions[NormalizeIndex(OtherVertex.mPositionIndex, PositionCount)] - FacePositions[NextPositionIndex]};

                CPVector.Normalize();
                NCVector.Normalize();
                PNVector.Normalize();
                OPVector.Normalize();
                OCVector.Normalize();
                ONVector.Normalize();

                //다른 정점들을 순회하다가 내부에 있는 정점이 나온다면 중단
                if (CPVector.Cross(OPVector).Dot(FaceNormal) > ConvexityEpsilon &&
                    NCVector.Cross(OCVector).Dot(FaceNormal) > ConvexityEpsilon &&
                    PNVector.Cross(ONVector).Dot(FaceNormal) > ConvexityEpsilon) {
                    OtherVertexCheck = true;
                    break;
                }
            }

            if (OtherVertexCheck) {
                FaceVertexIndex = (FaceVertexIndex + 1) % FaceVertics.size();
                Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[Load OBJ File] Vertex located inside the triangle");
                continue;
            }

            //여기까지 통과했으면 해당 삼각형을 추가하고 Current를 잘라낸다.
            for (int I{0}; I < 3; I++) {
                AddPNTIArray(FaceVertics[(FaceVertexIndex + I) % FaceVertics.size()], ObjInfo, OutGeometry, VertexCache);
            }

            //Current 제거
            FaceVertics.erase(FaceVertics.begin() + (FaceVertexIndex + 1) % FaceVertics.size());
        }

        //매우 작은 마이크로 쿼드와 같은 케이스에서는 Ear Clipping을 통과하지 못하고 무한 루프에 걸린다.
        //일정 수치 반복하면 break로 탈출하여 이쪽으로 오게된다.
        //여기까지 왔는데도 아직 3개보다 많다면 트라이앵글레이션으로 처리한다.
        if (FaceVertics.size() > 3) {
            for (std::size_t I{1}; I + 1 < FaceVertics.size(); I++) {
                AddPNTIArray(FaceVertics[0], ObjInfo, OutGeometry, VertexCache);
                AddPNTIArray(FaceVertics[I], ObjInfo, OutGeometry, VertexCache);
                AddPNTIArray(FaceVertics[I + 1], ObjInfo, OutGeometry, VertexCache);
            }
        } else {
            //남은 삼각형 1개만 남았다. 순서대로 입력
            for (auto& Face : FaceVertics) {
                AddPNTIArray(Face, ObjInfo, OutGeometry, VertexCache);
            }
        }

        const std::size_t GeneratedIndexCount{OutGeometry.mIndices.size() - FirstGeneratedIndex};

        if (GeneratedIndexCount > std::numeric_limits<Uint32>::max() - OutGeometry.mSubMeshIndexCounts[CurrentSubMeshIndex]) {
            return false;
        }

        OutGeometry.mSubMeshIndexCounts[CurrentSubMeshIndex] += static_cast<Uint32>(GeneratedIndexCount);
        CurrentSubMeshSourceIndexCount += FaceSourceIndexCount;

        if (CurrentSubMeshSourceIndexCount == ObjInfo.mSubMesh[CurrentSubMeshIndex]) {
            ++CurrentSubMeshIndex;
            CurrentSubMeshSourceIndexCount = 0;
        }
    }

    if (CurrentSubMeshIndex != ObjInfo.mSubMesh.size()) {
        return false;
    }

    if (OutGeometry.mPositions.empty() || OutGeometry.mIndices.empty()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Obj Load Failed. Resulting geometry is empty.");
        return false;
    }

    return true;
}

void FObjImporter::AddPNTIArray(const FFaceVertex& TargetVertex, const FObjInfo& ObjInfo, FGeometry& OutGeometry, std::unordered_map<FFaceVertexKey, Uint32, FFaceVertexKeyHash>& CacheMap) const {
    const Int32 PositionCount{static_cast<Int32>(ObjInfo.mPositions.size())};
    const Int32 UVCount{static_cast<Int32>(ObjInfo.mUVs.size())};
    const Int32 NormalCount{static_cast<Int32>(ObjInfo.mNormals.size())};

    //버텍스 배열에서 index
    const Int32 NormalizedPosition{NormalizeIndex(TargetVertex.mPositionIndex, PositionCount)};

    //vt/vn은 obj 상에서 생략 가능하므로, -1(생략)일 때는 정규화를 시도하지 않는다.
    const Int32 NormalizedUV{(TargetVertex.mUvIndex != -1) ? NormalizeIndex(TargetVertex.mUvIndex, UVCount) : -1};
    const Int32 NormalizedNormal{(TargetVertex.mNormalIndex != -1) ? NormalizeIndex(TargetVertex.mNormalIndex, NormalCount) : -1};

    if (NormalizedPosition < 0 || NormalizedPosition >= PositionCount) {
        return;
    }

    const FFaceVertexKey Key{NormalizedPosition, NormalizedUV, NormalizedNormal};

    const auto ExistingEntry{CacheMap.find(Key)};
    if (ExistingEntry != CacheMap.end()) {
        //이미 같은 조합의 정점이 있다면 새로 만들지 않고 인덱스만 재사용한다.
        OutGeometry.mIndices.push_back(ExistingEntry->second);
        return;
    }

    const Uint32 NewIndex{static_cast<Uint32>(OutGeometry.mPositions.size())};

    OutGeometry.mPositions.push_back(ObjInfo.mPositions[NormalizedPosition]);

    OutGeometry.mTexCoords.push_back((NormalizedUV >= 0 && NormalizedUV < UVCount) ? ObjInfo.mUVs[NormalizedUV] : FVector2{0.f, 0.f});

    OutGeometry.mNormals.push_back((NormalizedNormal >= 0 && NormalizedNormal < NormalCount) ? ObjInfo.mNormals[NormalizedNormal] : FVector{0.f, 0.f, 1.f});

    OutGeometry.mColors.push_back(ObjInfo.mColors[NormalizedPosition]);

    CacheMap.emplace(Key, NewIndex);
    OutGeometry.mIndices.push_back(NewIndex);
}

FVector FObjImporter::ComputeFaceNormal(const TArray<FFaceVertex>& PolygonVertices, const TArray<FVector>& Positions, Int32 PositionCount) const {
    FVector Normal{0.f, 0.f, 0.f};
    const std::size_t VertexCount{PolygonVertices.size()};

    for (std::size_t I{0}; I < VertexCount; I++) {
        const FVector& Current{Positions[NormalizeIndex(PolygonVertices[I].mPositionIndex, PositionCount)]};
        const FVector& Next{Positions[NormalizeIndex(PolygonVertices[(I + 1) % VertexCount].mPositionIndex, PositionCount)]};

        Normal.mX += (Current.mY - Next.mY) * (Current.mZ + Next.mZ);
        Normal.mY += (Current.mY - Next.mY) * (Current.mZ + Next.mZ);
        Normal.mZ += (Current.mY - Next.mY) * (Current.mZ + Next.mZ);
    }

    Normal.Normalize();
    return Normal;
}

TArray<FString> FObjImporter::SplitTokens(const FString& Line) {
    TArray<FString> Tokens{};
    std::istringstream Stream{Line};
    std::string Token{};
    while (Stream >> Token) {
        Tokens.push_back(FString{Token.c_str()});
    }
    return Tokens;
}

Int32 FObjImporter::NormalizeIndex(Int32 RawIndex, Int32 ArraySize) const {
    if (RawIndex > 0) {
        return RawIndex - 1;
    }

    //RawIndex가 음수라면 뒤에서부터 인덱스를 샌다.
    return ArraySize + RawIndex;
}
