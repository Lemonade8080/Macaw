#include "UMesh.h"
#include "pch.h"

#include "Core/Console/Console.h"
#include "Core/Base/FGuid.h"

#include "FObjImporter.h"
#include "Asset/Importer/FObjSerializer.h"
#include <ranges>
#include <windows.h>
#include <map>
#include <tuple>
#include <cmath>

namespace
{
    Uint64 MakeLODEdgeKey(Uint32 V0, Uint32 V1)
    {
        if (V0 > V1) { std::swap(V0, V1); }
        return (static_cast<Uint64>(V0) << 32) | V1;
    }

    void AddUniqueLODVertex(TArray<Uint32>& Vertices, Uint32 Vertex)
    {
        if (std::ranges::find(Vertices, Vertex) == Vertices.end()) { Vertices.push_back(Vertex); }
    }
}

std::size_t UMesh::GetAttributeIndex(EVertexAttribute Attribute) {
    return static_cast<std::size_t>(Attribute);
}

std::size_t UMesh::GetAttributeCount() {
    return static_cast<std::size_t>(EVertexAttribute::MAX);
}

UMesh::FGeneratedLOD* UMesh::GetGeneratedLOD(int Level)
{
    if (Level <= 0) { return nullptr; }

    const std::size_t Index{ static_cast<std::size_t>(Level - 1) };

    if (Index >= mGeneratedLODs.size()) { return nullptr; }

    return &mGeneratedLODs[Index];
}

const UMesh::FGeneratedLOD* UMesh::GetGeneratedLOD(int Level) const
{
    if (Level <= 0) { return nullptr; }

    const std::size_t Index{ static_cast<std::size_t>(Level - 1) };

    if (Index >= mGeneratedLODs.size()) { return nullptr; }

    return &mGeneratedLODs[Index];
}

bool UMesh::BuildBoundingBoxFromMesh()
{
    const auto Positions{ GetVertexAttributeData<EVertexAttribute::Position>() };
    if (Positions.empty()) {
        return false;
    }

    std::vector<DirectX::XMFLOAT3> Points{};
    Points.reserve(Positions.size());
    for (const FVector3& Position : Positions) {
        Points.emplace_back(Position.mX, Position.mY, Position.mZ);
    }

    DirectX::BoundingBox Bounds{};
    DirectX::BoundingBox::CreateFromPoints(Bounds, Points.size(), Points.data(), sizeof(DirectX::XMFLOAT3));
    DirectX::BoundingOrientedBox::CreateFromBoundingBox(mBoundingBox, Bounds);
    return true;
}

bool UMesh::Initialize(ID3D11Device* Device, const std::filesystem::path& SourceObjPath, const std::filesystem::path& BinaryPath, const FMaterialResolver& MaterialResolver, const FMaterialGroupResolver& MaterialGroupResolver, bool FlipUV) {
    if (Device == nullptr || (SourceObjPath.empty() && BinaryPath.empty())) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model load rejected: device or asset path is invalid.");
        return false;
    }

    FObjImporter ObjImporter{};
    FGeometry Geometry{};

    std::error_code FileSystemError{};
    const bool BHasBinary{!BinaryPath.empty() && std::filesystem::is_regular_file(BinaryPath, FileSystemError)};
    Uint32 LoadedVersion{};
    const bool BLoadedFromBinary{BHasBinary && FObjSerializer::LoadBinary(BinaryPath.string().c_str(), Geometry, LoadedVersion)};
    bool BHasRawGeometry{BLoadedFromBinary && LoadedVersion == FObjSerializer::CurrentVersion};

    if (BLoadedFromBinary && !BHasRawGeometry && !SourceObjPath.empty() && std::filesystem::is_regular_file(SourceObjPath, FileSystemError)) {
        FGeometry RawGeometry{};
        if (ObjImporter.LoadObjFile(SourceObjPath.string().c_str(), RawGeometry)) {
            const FString TemporarySuffix{FGuid::NewGuid().ToString()};
            const std::filesystem::path TemporaryBinaryPath{BinaryPath.string() + "." + TemporarySuffix.c_str() + ".tmp"};
            if (FObjSerializer::SaveBinary(RawGeometry, TemporaryBinaryPath.string().c_str()) && MoveFileExW(TemporaryBinaryPath.c_str(), BinaryPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                Geometry = std::move(RawGeometry);
                BHasRawGeometry = true;
                Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Rebuilt legacy model binary with original UVs: %s", BinaryPath.generic_string().c_str());
            } else {
                Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Failed to rebuild legacy model binary: %s", BinaryPath.generic_string().c_str());
                std::filesystem::remove(TemporaryBinaryPath, FileSystemError);
            }
        }
    }

    if (BLoadedFromBinary) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Loaded model binary: %s", BinaryPath.generic_string().c_str());
    } else {
        if (SourceObjPath.empty()) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to load standalone model binary: %s", BinaryPath.generic_string().c_str());
            return false;
        }

        if (BHasBinary) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "[UMesh] Failed to load model binary; Maybe Different Version. falling back to OBJ: %s", BinaryPath.generic_string().c_str());
        }

        Geometry = FGeometry{};
        if (!ObjImporter.LoadObjFile(SourceObjPath.string().c_str(), Geometry)) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[UMesh] Failed to import OBJ geometry: %s", SourceObjPath.generic_string().c_str());
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[UMesh] Import Failed. Check Obj File Path : %s", SourceObjPath.generic_string().c_str());
            return false;
        }
        BHasRawGeometry = true;

        if (!BinaryPath.empty() && !FObjSerializer::SaveBinary(Geometry, BinaryPath.string().c_str())) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "[UMesh] Failed to create model binary: %s", BinaryPath.generic_string().c_str());
        } else if (!BinaryPath.empty()) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[UMesh] Created model binary: %s", BinaryPath.generic_string().c_str());
        }
    }

    if (FlipUV && BHasRawGeometry) {
        for (FVector2& UV : Geometry.mTexCoords) {
            UV.mY = 1.0f - UV.mY;
        }
    }

    const std::filesystem::path AssetPath{BLoadedFromBinary ? BinaryPath : SourceObjPath};
    if (!UAsset::Initialize(Device, AssetPath)) {
        return false;
    }

    if (BLoadedFromBinary && Geometry.mSubMeshIndexCounts.empty() && !Geometry.mIndices.empty()) {
        Geometry.mSubMeshIndexCounts.push_back(static_cast<Uint32>(Geometry.mIndices.size()));
    }

    if (Geometry.mMaterialNames.empty() && Geometry.mSubMeshIndexCounts.size() == 1) {
        Geometry.mMaterialNames.push_back({});
    }

    FAssetHandle ImportedMaterial{};

    if (!Geometry.mMaterialFileName.empty()) {
        const std::filesystem::path MaterialPath{(AssetPath.parent_path() / std::filesystem::path(Geometry.mMaterialFileName.c_str())).lexically_normal()};
        if (MaterialResolver) {
            ImportedMaterial = MaterialResolver(MaterialPath);
        }

        if (!ImportedMaterial) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model MTL asset was not found; using material group 0: %s", MaterialPath.generic_string().c_str());
        }
    }

    TArray<FSubMesh> ImportedSubMeshes{};
    ImportedSubMeshes.reserve(Geometry.mSubMeshIndexCounts.size());

    if (Geometry.mMaterialNames.size() != Geometry.mSubMeshIndexCounts.size()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model material group count does not match submesh count: %s", AssetPath.generic_string().c_str());
        return false;
    }

    Uint32 FirstIndex{0};

    for (Uint32 SubMeshIndex{0}; SubMeshIndex < Geometry.mSubMeshIndexCounts.size(); ++SubMeshIndex) {
        FSubMesh SubMesh{};
        SubMesh.mFirstIndex = FirstIndex;
        SubMesh.mIndexCount = Geometry.mSubMeshIndexCounts[SubMeshIndex];

        if (SubMesh.mFirstIndex > Geometry.mIndices.size() || SubMesh.mIndexCount > Geometry.mIndices.size() - SubMesh.mFirstIndex) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Model submesh index range is invalid: %s", AssetPath.generic_string().c_str());
            return false;
        }

        FirstIndex += SubMesh.mIndexCount;

        const FString& MaterialName{Geometry.mMaterialNames[SubMeshIndex]};
        if (!MaterialName.empty()) {
            if (ImportedMaterial && MaterialGroupResolver) {
                const std::optional<Uint32> MaterialGroupIndex{MaterialGroupResolver(ImportedMaterial, MaterialName)};
                if (MaterialGroupIndex.has_value()) {
                    SubMesh.mMaterialGroupIndex = *MaterialGroupIndex;
                } else {
                    Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model MTL group was not found; using material group 0: %s in %s", MaterialName.c_str(), AssetPath.generic_string().c_str());
                }
            } else {
                Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Model has no usable MTL; using material group 0: %s", AssetPath.generic_string().c_str());
            }
        }

        ImportedSubMeshes.push_back(SubMesh);
    }

    if (FirstIndex != Geometry.mIndices.size() || !Make(Device, Geometry.mIndices,
            MakeVertexAttribute<EVertexAttribute::Position>(Geometry.mPositions),
            MakeVertexAttribute<EVertexAttribute::Normal>(Geometry.mNormals),
            MakeVertexAttribute<EVertexAttribute::UV>(Geometry.mTexCoords),
            MakeVertexAttribute<EVertexAttribute::Color>(Geometry.mColors))) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "Failed to create GPU buffers for model: %s", AssetPath.generic_string().c_str());
        return false;
    }

    if (!BuildBoundingBoxFromMesh()) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Failed to create a Bounding Box for model: %s", AssetPath.generic_string().c_str());
    }

    if (!RaycastAccelerationStructure.BuildStructure(*this)) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Failed to create a RaycastAS for model: %s", AssetPath.generic_string().c_str());
    }

    mSubMeshes = std::move(ImportedSubMeshes);

    return true;
}

ID3D11Buffer* UMesh::GetVertexBuffer(EVertexAttribute Attribute) const {
    const std::size_t Index{GetAttributeIndex(Attribute)};

    if (Index >= GetAttributeCount()) {
        return nullptr;
    }

    return mVertexBuffers[Index].Get();
}

ID3D11Buffer* UMesh::GetVertexBuffer(EVertexAttribute Attribute, int Level) const
{
    const FGeneratedLOD* LOD{ GetGeneratedLOD(Level) };

    if (LOD && LOD->IsValid())
    {
        if (Attribute == EVertexAttribute::Position) { return LOD->mVertexBuffer.Get(); }
        if (Attribute == EVertexAttribute::Normal && LOD->mNormalBuffer) { return LOD->mNormalBuffer.Get(); }
        if (Attribute == EVertexAttribute::UV && LOD->mUVBuffer) { return LOD->mUVBuffer.Get(); }
    }

    return GetVertexBuffer(Attribute);
}

ID3D11Buffer* UMesh::GetIndexBuffer() const {
    return mIndexBuffer.Get();
}

ID3D11Buffer* UMesh::GetIndexBuffer(int Level) const
{
    const FGeneratedLOD* LOD{ GetGeneratedLOD(Level) };

    if (LOD != nullptr && LOD->IsValid()) {
        return LOD->mIndexBuffer.Get();
    }

    return GetIndexBuffer();
}

Uint32 UMesh::GetIndexCount(int Level) const
{
    const FGeneratedLOD* LOD{ GetGeneratedLOD(Level) };

    if (LOD && LOD->IsValid())
        return LOD->mIndexCount;

    return static_cast<Uint32>((std::min)(mIndices.size(), static_cast<std::size_t>(UINT32_MAX)));
}

bool UMesh::HasLOD(int Level) const
{
    if (Level == 0)
        return mIndexBuffer && !mIndices.empty();

    const FGeneratedLOD* LOD{ GetGeneratedLOD(Level) };
    return LOD && LOD->IsValid();
}

Uint64 UMesh::GetRenderRevision() const {
    return mRenderRevision;
}

bool UMesh::HasVertexAttribute(EVertexAttribute Attribute) const {
    const std::size_t Index{GetAttributeIndex(Attribute)};

    if (Index >= GetAttributeCount()) {
        return false;
    }

    return mAttributeStorage[Index] != nullptr;
}

Uint32 UMesh::GetVertexStride(EVertexAttribute Attribute) const {
    const std::size_t Index{GetAttributeIndex(Attribute)};

    if (Index >= GetAttributeCount() || !mAttributeStorage[Index]) {
        return 0;
    }

    return mAttributeStorage[Index]->GetStride();
}

Uint32 UMesh::GetVertexAttributeCount(EVertexAttribute Attribute) const {
    const std::size_t Index{GetAttributeIndex(Attribute)};

    if (Index >= GetAttributeCount() || !mAttributeStorage[Index]) {
        return 0;
    }

    return mAttributeStorage[Index]->GetCount();
}

const void* UMesh::GetVertexData(EVertexAttribute Attribute) const {
    const std::size_t Index{GetAttributeIndex(Attribute)};

    if (Index >= GetAttributeCount() || !mAttributeStorage[Index]) {
        return nullptr;
    }

    return mAttributeStorage[Index]->GetData();
}

bool UMesh::GenerateLOD(ID3D11Device* Device, Uint32 Level, float TargetRatio) {

    if (Device == nullptr || Level == 0 || !std::isfinite(TargetRatio) ||
        TargetRatio <= 0.0f || TargetRatio >= 1.0f) { return false; }

    // 1. 기하 정점을 통합하고 UV 이음매, Normal 경계, 인접 삼각형을 준비한다.
    FLODGeometry Geometry{};
    if (!BuildLODGeometry(Geometry)) { return false; }

    // 2. 목표 삼각형 수까지 안전한 Edge를 Collapse한다.
    const Uint32 OriginalTriangleCount{ static_cast<Uint32>(Geometry.mIndices.size() / 3) };
    const Uint32 TargetTriangleCount{ (std::max)(1u, static_cast<Uint32>(OriginalTriangleCount * TargetRatio)) };
    SimplifyLODGeometry(Geometry, TargetTriangleCount);

    // 3. 렌더 정점에 속성을 반영하고 GPU 버퍼를 만든다.
    FGeneratedLOD NewLOD{};
    const bool BNeedsSectionReorder{ Geometry.mSubMeshes.size() > 1 && Geometry.mSubMeshes.size() < mSubMeshes.size() };
    if (Geometry.mIndices.size() == mIndices.size() && !BNeedsSectionReorder)
    {
        // 줄어든 면이 없으면 Normal 재계산과 중복 업로드 없이 원본 버퍼를 공유한다.
        NewLOD.mVertexBuffer = mVertexBuffers[GetAttributeIndex(EVertexAttribute::Position)];
        NewLOD.mNormalBuffer = mVertexBuffers[GetAttributeIndex(EVertexAttribute::Normal)];
        NewLOD.mUVBuffer = mVertexBuffers[GetAttributeIndex(EVertexAttribute::UV)];
        NewLOD.mIndexBuffer = mIndexBuffer;
        NewLOD.mIndexCount = static_cast<Uint32>(mIndices.size());
        NewLOD.mSubMeshes = std::move(Geometry.mSubMeshes);
    }
    else
    {
        const FLODRenderData RenderData{ BuildLODRenderData(Geometry) };
        if (!CreateLODBuffers(Device, RenderData, NewLOD)) { return false; }
        NewLOD.mSubMeshes = std::move(Geometry.mSubMeshes);
    }

    // 4. 완성된 결과만 교체한다. 중간에 실패하면 기존 LOD와 Revision을 유지한다.
    if (mGeneratedLODs.size() < Level) { mGeneratedLODs.resize(Level); }
    mGeneratedLODs[Level - 1] = std::move(NewLOD);
    ++mRenderRevision;
    return true;
}

bool UMesh::BuildLODSubMeshes(FLODGeometry& Geometry) const
{
    const Uint32 IndexCount{ static_cast<Uint32>(mIndices.size()) };
    if (mSubMeshes.empty())
    {
        Geometry.mSubMeshes.push_back({ 0, 0, 0, IndexCount });
        return true;
    }

    std::unordered_map<Uint32, Uint32> MaterialSections{};
    TArray<Uint32> SourceSections{};
    SourceSections.reserve(mSubMeshes.size());
    Uint32 NextIndex{};
    for (const FSubMesh& SubMesh : mSubMeshes)
    {
        // 원본 구간이 삼각형 단위로 전체 인덱스를 빠짐없이 덮어야 한다.
        if (SubMesh.mFirstIndex != NextIndex || SubMesh.mIndexCount % 3 != 0 ||
            SubMesh.mIndexCount > IndexCount - NextIndex) { return false; }
        NextIndex += SubMesh.mIndexCount;
        const auto [It, BInserted]{ MaterialSections.try_emplace(SubMesh.mMaterialGroupIndex,
            static_cast<Uint32>(Geometry.mSubMeshes.size())) };
        if (BInserted) { Geometry.mSubMeshes.push_back({ 0, 0, SubMesh.mMaterialGroupIndex, 0 }); }
        Geometry.mSubMeshes[It->second].mSourceIndexCount += SubMesh.mIndexCount;
        SourceSections.push_back(It->second);
    }
    if (NextIndex != IndexCount) { return false; }
    if (Geometry.mSubMeshes.size() == 1) { return true; }

    Geometry.mTriangleSubMeshes.resize(IndexCount / 3);
    for (std::size_t Index{}; Index < mSubMeshes.size(); ++Index)
    {
        const FSubMesh& SubMesh{ mSubMeshes[Index] };
        const auto First{ Geometry.mTriangleSubMeshes.begin() + SubMesh.mFirstIndex / 3 };
        std::fill_n(First, SubMesh.mIndexCount / 3, SourceSections[Index]);
    }
    return true;
}

bool UMesh::BuildLODGeometry(FLODGeometry& Geometry) const
{
    const auto SourcePositions{ GetVertexAttributeData<EVertexAttribute::Position>() };
    const auto SourceNormals{ GetVertexAttributeData<EVertexAttribute::Normal>() };
    const auto SourceUVs{ GetVertexAttributeData<EVertexAttribute::UV>() };

    if (SourcePositions.empty() || SourcePositions.size() > UINT32_MAX ||
        mIndices.size() < 3 || mIndices.size() % 3 != 0 ||
        mIndices.size() > UINT32_MAX) { return false; }
    if (!SourceNormals.empty() && SourceNormals.size() != SourcePositions.size()) { return false; }
    if (!SourceUVs.empty() && SourceUVs.size() != SourcePositions.size()) { return false; }
    Geometry.mSourceUVs = SourceUVs;
    if (!BuildLODSubMeshes(Geometry)) { return false; }

    // 같은 위치의 렌더 정점을 하나의 기하 정점으로 묶는다.
    std::map<std::tuple<float, float, float>, Uint32> PositionMap{};
    std::map<std::tuple<Uint32, float, float, float>, Uint32> NormalMap{};
    std::map<std::tuple<Uint32, Uint32, float, float>, Uint32> AttributeMap{};

    Geometry.mPositions.reserve(SourcePositions.size());
    Geometry.mVertexGroupCounts.reserve(SourcePositions.size());
    Geometry.mAttributeGroups.reserve(SourcePositions.size());
    Geometry.mRenderGroups.resize(SourcePositions.size());
    Geometry.mNormalParents.reserve(SourceNormals.size());
    Geometry.mNormalRanks.reserve(SourceNormals.size());

    TArray<Uint32> RenderToGeometry(SourcePositions.size());

    for (Uint32 RenderVertex{}; RenderVertex < SourcePositions.size(); ++RenderVertex)
    {
        const FVector3& Position{ SourcePositions[RenderVertex] };
        if (!std::isfinite(Position.X) || !std::isfinite(Position.Y) || !std::isfinite(Position.Z)) { return false; }
        const auto Key{ std::make_tuple(Position.X, Position.Y, Position.Z) };
        const auto [It, BInserted]{ PositionMap.try_emplace(Key, static_cast<Uint32>(Geometry.mPositions.size())) };
        const Uint32 GeometryIndex{ It->second };
        RenderToGeometry[RenderVertex] = GeometryIndex;

        if (BInserted)
        {
            Geometry.mPositions.push_back(Position);
            Geometry.mVertexGroupCounts.push_back(0);
        }

        const FVector2D UV{ SourceUVs.empty() ? FVector2D{} : SourceUVs[RenderVertex] };
        if (!std::isfinite(UV.X) || !std::isfinite(UV.Y)) { return false; }

        // 원본 Normal이 같은 중복 정점은 함께 평균내고, 다른 Normal은 경계로 유지한다.
        Uint32 NormalGroup{ UINT32_MAX };
        if (!SourceNormals.empty())
        {
            const FVector3& Normal{ SourceNormals[RenderVertex] };
            if (!std::isfinite(Normal.X) || !std::isfinite(Normal.Y) || !std::isfinite(Normal.Z)) { return false; }
            const auto NormalKey{ std::make_tuple(GeometryIndex, Normal.X, Normal.Y, Normal.Z) };
            const auto [NormalIt, BNewNormal]{ NormalMap.try_emplace(NormalKey, static_cast<Uint32>(Geometry.mNormalParents.size())) };
            NormalGroup = NormalIt->second;
            if (BNewNormal)
            {
                Geometry.mNormalParents.push_back(NormalGroup);
                Geometry.mNormalRanks.push_back(0);
            }
        }

        // Position만 통합한다. UV 또는 Normal이 다르면 별도 그룹으로 기록한다.
        const auto AttributeKey{ std::make_tuple(GeometryIndex, NormalGroup, UV.X, UV.Y) };
        const auto [AttributeIt, BNewAttribute]{ AttributeMap.try_emplace(AttributeKey, static_cast<Uint32>(Geometry.mAttributeGroups.size())) };
        if (BNewAttribute) { Geometry.mAttributeGroups.push_back({ UV, NormalGroup }); }
        Geometry.mRenderGroups[RenderVertex] = AttributeIt->second;
    }

    Geometry.mIndices.reserve(mIndices.size());
    Geometry.mRenderIndices.reserve(mIndices.size());
    Geometry.mFirstCorners.assign(Geometry.mPositions.size(), UINT32_MAX);
    Geometry.mNextCorners.resize(mIndices.size());
    Geometry.mPreviousCorners.resize(mIndices.size());

    for (std::size_t Index{}; Index < mIndices.size(); Index += 3)
    {
        const Uint32 R0{ mIndices[Index] };
        const Uint32 R1{ mIndices[Index + 1] };
        const Uint32 R2{ mIndices[Index + 2] };
        if (R0 >= RenderToGeometry.size() || R1 >= RenderToGeometry.size() || R2 >= RenderToGeometry.size()) { return false; }

        const Uint32 I0{ RenderToGeometry[R0] };
        const Uint32 I1{ RenderToGeometry[R1] };
        const Uint32 I2{ RenderToGeometry[R2] };
        if (I0 == I1 || I1 == I2 || I2 == I0) { continue; }

        if (!Geometry.mTriangleSubMeshes.empty())
        {
            Geometry.mTriangleSubMeshes[Geometry.mIndices.size() / 3] = Geometry.mTriangleSubMeshes[Index / 3];
        }

        for (Uint32 RenderIndex : { R0, R1, R2 })
        {
            const Uint32 Corner{ static_cast<Uint32>(Geometry.mIndices.size()) };
            Geometry.mIndices.push_back(RenderToGeometry[RenderIndex]);
            Geometry.mRenderIndices.push_back(RenderIndex);
            AttachLODCorner(Geometry, Corner);
        }
    }

    Geometry.mNextCorners.resize(Geometry.mIndices.size());
    Geometry.mPreviousCorners.resize(Geometry.mIndices.size());
    Geometry.mActiveTriangles.assign(Geometry.mIndices.size() / 3, 1);
    TArray<Uint32> Groups{};
    for (Uint32 Vertex{}; Vertex < Geometry.mPositions.size(); ++Vertex) { RefreshLODVertexGroups(Geometry, Vertex, Groups); }
    if (!Geometry.mTriangleSubMeshes.empty())
    {
        Geometry.mTriangleSubMeshes.resize(Geometry.mIndices.size() / 3);
        Geometry.mMaterialBoundaryVertices.resize(Geometry.mPositions.size());
        // 서로 다른 재질의 접점만 고정한다. 내부 정점은 이 경계 쪽으로 합칠 수 있다.
        for (Uint32 Vertex{}; Vertex < Geometry.mPositions.size(); ++Vertex)
        {
            const Uint32 First{ Geometry.mFirstCorners[Vertex] };
            if (First == UINT32_MAX) { continue; }
            const Uint32 Section{ Geometry.mTriangleSubMeshes[First / 3] };
            for (Uint32 Corner{ Geometry.mNextCorners[First] }; Corner != UINT32_MAX; Corner = Geometry.mNextCorners[Corner])
            {
                if (Geometry.mTriangleSubMeshes[Corner / 3] != Section)
                {
                    Geometry.mMaterialBoundaryVertices[Vertex] = 1;
                    break;
                }
            }
        }
    }
    return !Geometry.mIndices.empty();
}

void UMesh::FLODQuadric::AddPlane(double X, double Y, double Z, double D)
{
    mValues[0] += X * X; mValues[1] += X * Y; mValues[2] += X * Z; mValues[3] += X * D;
    mValues[4] += Y * Y; mValues[5] += Y * Z; mValues[6] += Y * D;
    mValues[7] += Z * Z; mValues[8] += Z * D; mValues[9] += D * D;
}

UMesh::FLODQuadric& UMesh::FLODQuadric::operator+=(const FLODQuadric& Other)
{
    for (std::size_t Index{}; Index < mValues.size(); ++Index) { mValues[Index] += Other.mValues[Index]; }
    return *this;
}

double UMesh::FLODQuadric::Evaluate(const FVector3& Position, const FVector3& Origin) const
{
    const double X{ static_cast<double>(Position.X) - Origin.X };
    const double Y{ static_cast<double>(Position.Y) - Origin.Y };
    const double Z{ static_cast<double>(Position.Z) - Origin.Z };
    const double Error{ mValues[0] * X * X + mValues[4] * Y * Y + mValues[7] * Z * Z + mValues[9] +
        2.0 * (mValues[1] * X * Y + mValues[2] * X * Z + mValues[3] * X +
            mValues[5] * Y * Z + mValues[6] * Y + mValues[8] * Z) };
    // 합산 과정에서 생기는 미세한 음수 오차만 보정한다.
    return Error < 0.0 ? 0.0 : Error;
}

float UMesh::FLODQuadric::FindEdgeInterpolation(const FVector3& P0, const FVector3& P1, const FVector3& Origin) const
{
    const double X{ static_cast<double>(P0.X) - Origin.X };
    const double Y{ static_cast<double>(P0.Y) - Origin.Y };
    const double Z{ static_cast<double>(P0.Z) - Origin.Z };
    const double DX{ static_cast<double>(P1.X) - P0.X };
    const double DY{ static_cast<double>(P1.Y) - P0.Y };
    const double DZ{ static_cast<double>(P1.Z) - P0.Z };
    const double AX{ mValues[0] * DX + mValues[1] * DY + mValues[2] * DZ };
    const double AY{ mValues[1] * DX + mValues[4] * DY + mValues[5] * DZ };
    const double AZ{ mValues[2] * DX + mValues[5] * DY + mValues[7] * DZ };
    const double Curvature{ DX * AX + DY * AY + DZ * AZ };
    const double Scale{ (mValues[0] + mValues[4] + mValues[7]) * (DX * DX + DY * DY + DZ * DZ) };

    // P(t) = P0 + t(P1-P0)에서 QEM 오차를 최소화한다. 평면 위의 자유 방향은 중간점을 쓴다.
    if (Curvature <= Scale * 1e-12) { return 0.5f; }
    const double Slope{ X * AX + Y * AY + Z * AZ + mValues[3] * DX + mValues[6] * DY + mValues[8] * DZ };
    const double T{ -Slope / Curvature };
    return std::isfinite(T) ? static_cast<float>(std::clamp(T, 0.0, 1.0)) : 0.5f;
}

void UMesh::SimplifyLODGeometry(FLODGeometry& Geometry, Uint32 TargetTriangleCount)
{
    FLODSimplification State{};
    State.mTriangleCount = static_cast<Uint32>(Geometry.mIndices.size() / 3);
    State.mEdgeFaceCounts.reserve(Geometry.mIndices.size() / 2);
    State.mBoundaryEdgeCounts.resize(Geometry.mPositions.size());
    State.mNonManifoldEdgeCounts.resize(Geometry.mPositions.size());
    State.mVertexVersions.resize(Geometry.mPositions.size());
    State.mQuadrics.resize(Geometry.mPositions.size());
    State.mQuadricOrigin = Geometry.mPositions.front();

    // Edge 연결 수와 Heap은 처음에 한 번만 만든다.
    for (std::size_t Index{}; Index < Geometry.mIndices.size(); Index += 3)
    {
        const Uint32 I0{ Geometry.mIndices[Index] };
        const Uint32 I1{ Geometry.mIndices[Index + 1] };
        const Uint32 I2{ Geometry.mIndices[Index + 2] };
        UpdateLODEdge(State, I0, I1, true);
        UpdateLODEdge(State, I1, I2, true);
        UpdateLODEdge(State, I2, I0, true);

        // 원본 면의 평면 오차를 한 번만 만든다. 원점 이동과 double 계산으로 상쇄 오차를 줄인다.
        const FVector3& P0{ Geometry.mPositions[I0] };
        const FVector3& P1{ Geometry.mPositions[I1] };
        const FVector3& P2{ Geometry.mPositions[I2] };
        const double AX{ static_cast<double>(P1.X) - P0.X }, AY{ static_cast<double>(P1.Y) - P0.Y }, AZ{ static_cast<double>(P1.Z) - P0.Z };
        const double BX{ static_cast<double>(P2.X) - P0.X }, BY{ static_cast<double>(P2.Y) - P0.Y }, BZ{ static_cast<double>(P2.Z) - P0.Z };
        double NX{ AY * BZ - AZ * BY }, NY{ AZ * BX - AX * BZ }, NZ{ AX * BY - AY * BX };
        const double LengthSquared{ NX * NX + NY * NY + NZ * NZ };
        if (LengthSquared <= 0.0 || !std::isfinite(LengthSquared)) { continue; }
        const double InvLength{ 1.0 / std::sqrt(LengthSquared) };
        NX *= InvLength; NY *= InvLength; NZ *= InvLength;
        const double D{ -(NX * (static_cast<double>(P0.X) - State.mQuadricOrigin.X) +
            NY * (static_cast<double>(P0.Y) - State.mQuadricOrigin.Y) + NZ * (static_cast<double>(P0.Z) - State.mQuadricOrigin.Z)) };
        FLODQuadric FaceQuadric{};
        FaceQuadric.AddPlane(NX, NY, NZ, D);
        State.mQuadrics[I0] += FaceQuadric;
        State.mQuadrics[I1] += FaceQuadric;
        State.mQuadrics[I2] += FaceQuadric;
    }

    State.mCandidates.reserve(State.mEdgeFaceCounts.size());
    for (const auto& [Key, FaceCount] : State.mEdgeFaceCounts)
    {
        FLODCandidate Candidate{};
        if (MakeLODCandidate(Key, Geometry, State, Candidate)) { State.mCandidates.push_back(Candidate); }
    }
    std::ranges::make_heap(State.mCandidates, CompareLODCandidates);

    while (State.mTriangleCount > TargetTriangleCount && !State.mCandidates.empty())
    {
        std::ranges::pop_heap(State.mCandidates, CompareLODCandidates);
        const FLODCandidate Candidate{ State.mCandidates.back() };
        State.mCandidates.pop_back();

        // 주변 면이 바뀐 후보는 폐기한다. 최신 후보는 갱신 시 이미 Heap에 추가했다.
        if (Candidate.mVersionV0 != State.mVertexVersions[Candidate.mV0] ||
            Candidate.mVersionV1 != State.mVertexVersions[Candidate.mV1]) { continue; }

        FLODCollapse Collapse{};
        if (!PrepareLODCollapse(Candidate, Geometry, State, Collapse) ||
            !CanCollapseEdge(Collapse, Geometry, State)) { continue; }

        ApplyLODCollapse(Collapse, Geometry, State);

        // 오래된 후보가 과도하게 쌓일 때만 Heap을 압축해 임시 메모리를 제한한다.
        if (State.mCandidates.size() > State.mEdgeFaceCounts.size() * 4 + 128)
        {
            std::erase_if(State.mCandidates, [&State](const FLODCandidate& Entry)
            {
                return Entry.mVersionV0 != State.mVertexVersions[Entry.mV0] ||
                    Entry.mVersionV1 != State.mVertexVersions[Entry.mV1];
            });
            std::ranges::make_heap(State.mCandidates, CompareLODCandidates);
        }
    }

    // 삭제된 삼각형의 실제 배열 정리는 모든 Collapse가 끝난 뒤 한 번만 한다.
    std::size_t WriteIndex{};
    for (std::size_t Triangle{}; Triangle < Geometry.mActiveTriangles.size(); ++Triangle)
    {
        if (!Geometry.mActiveTriangles[Triangle]) { continue; }
        const Uint32 Section{ Geometry.mTriangleSubMeshes.empty() ? 0 : Geometry.mTriangleSubMeshes[Triangle] };
        Geometry.mSubMeshes[Section].mIndexCount += 3;
        if (!Geometry.mTriangleSubMeshes.empty()) { Geometry.mTriangleSubMeshes[WriteIndex / 3] = Section; }
        for (Uint32 Corner{}; Corner < 3; ++Corner)
        {
            Geometry.mIndices[WriteIndex] = Geometry.mIndices[Triangle * 3 + Corner];
            Geometry.mRenderIndices[WriteIndex] = Geometry.mRenderIndices[Triangle * 3 + Corner];
            ++WriteIndex;
        }
    }
    Geometry.mIndices.resize(WriteIndex);
    Geometry.mRenderIndices.resize(WriteIndex);
    if (!Geometry.mTriangleSubMeshes.empty()) { Geometry.mTriangleSubMeshes.resize(WriteIndex / 3); }
    Uint32 FirstIndex{};
    for (FSubMesh& SubMesh : Geometry.mSubMeshes)
    {
        SubMesh.mFirstIndex = FirstIndex;
        FirstIndex += SubMesh.mIndexCount;
    }
    // Corner 번호가 달라졌으므로 생성 중 사용한 인접 목록은 더 이상 참조하지 않는다.
    Geometry.mFirstCorners.clear();
    Geometry.mNextCorners.clear();
    Geometry.mPreviousCorners.clear();
    Geometry.mActiveTriangles.clear();
}

bool UMesh::CompareLODCandidates(const FLODCandidate& Left, const FLODCandidate& Right)
{
    if (Left.mCost != Right.mCost) { return Left.mCost > Right.mCost; }
    if (Left.mLengthSquared != Right.mLengthSquared) { return Left.mLengthSquared > Right.mLengthSquared; }
    if (Left.mV0 != Right.mV0) { return Left.mV0 > Right.mV0; }
    return Left.mV1 > Right.mV1;
}

void UMesh::UpdateLODEdge(FLODSimplification& State, Uint32 V0, Uint32 V1, bool BAddFace)
{
    const Uint64 Key{ MakeLODEdgeKey(V0, V1) };
    auto It{ State.mEdgeFaceCounts.find(Key) };
    const Uint32 OldCount{ It != State.mEdgeFaceCounts.end() ? It->second : 0 };

    if (OldCount == 1) { --State.mBoundaryEdgeCounts[V0]; --State.mBoundaryEdgeCounts[V1]; }
    if (OldCount > 2) { --State.mNonManifoldEdgeCounts[V0]; --State.mNonManifoldEdgeCounts[V1]; }

    const Uint32 NewCount{ BAddFace ? OldCount + 1 : OldCount - 1 };
    if (NewCount == 0) { State.mEdgeFaceCounts.erase(It); }
    else if (It == State.mEdgeFaceCounts.end()) { State.mEdgeFaceCounts.emplace(Key, NewCount); }
    else { It->second = NewCount; }

    if (NewCount == 1) { ++State.mBoundaryEdgeCounts[V0]; ++State.mBoundaryEdgeCounts[V1]; }
    if (NewCount > 2) { ++State.mNonManifoldEdgeCounts[V0]; ++State.mNonManifoldEdgeCounts[V1]; }
}

void UMesh::DetachLODCorner(FLODGeometry& Geometry, Uint32 Corner)
{
    const Uint32 Previous{ Geometry.mPreviousCorners[Corner] };
    const Uint32 Next{ Geometry.mNextCorners[Corner] };
    if (Previous == UINT32_MAX) { Geometry.mFirstCorners[Geometry.mIndices[Corner]] = Next; }
    else { Geometry.mNextCorners[Previous] = Next; }
    if (Next != UINT32_MAX) { Geometry.mPreviousCorners[Next] = Previous; }
}

void UMesh::AttachLODCorner(FLODGeometry& Geometry, Uint32 Corner)
{
    const Uint32 Vertex{ Geometry.mIndices[Corner] };
    const Uint32 First{ Geometry.mFirstCorners[Vertex] };
    Geometry.mPreviousCorners[Corner] = UINT32_MAX;
    Geometry.mNextCorners[Corner] = First;
    if (First != UINT32_MAX) { Geometry.mPreviousCorners[First] = Corner; }
    Geometry.mFirstCorners[Vertex] = Corner;
}

void UMesh::RefreshLODVertexGroups(FLODGeometry& Geometry, Uint32 Vertex, TArray<Uint32>& Groups)
{
    Groups.clear();
    for (Uint32 Corner{ Geometry.mFirstCorners[Vertex] }; Corner != UINT32_MAX; Corner = Geometry.mNextCorners[Corner])
    {
        AddUniqueLODVertex(Groups, Geometry.mRenderGroups[Geometry.mRenderIndices[Corner]]);
    }
    Geometry.mVertexGroupCounts[Vertex] = static_cast<Uint32>(Groups.size());
}

Uint32 UMesh::FindLODNormalGroup(const FLODGeometry& Geometry, Uint32 Group)
{
    if (Group == UINT32_MAX) { return Group; }
    while (Geometry.mNormalParents[Group] != Group) { Group = Geometry.mNormalParents[Group]; }
    return Group;
}

void UMesh::MergeLODNormalGroups(FLODGeometry& Geometry, Uint32 Group0, Uint32 Group1)
{
    Group0 = FindLODNormalGroup(Geometry, Group0);
    Group1 = FindLODNormalGroup(Geometry, Group1);
    if (Group0 == Group1 || Group0 == UINT32_MAX || Group1 == UINT32_MAX) { return; }
    if (Geometry.mNormalRanks[Group0] < Geometry.mNormalRanks[Group1]) { std::swap(Group0, Group1); }
    Geometry.mNormalParents[Group1] = Group0;
    if (Geometry.mNormalRanks[Group0] == Geometry.mNormalRanks[Group1]) { ++Geometry.mNormalRanks[Group0]; }
}

bool UMesh::MakeLODCandidate(Uint64 Key, const FLODGeometry& Geometry, const FLODSimplification& State, FLODCandidate& Candidate) const
{
    const Uint32 V0{ static_cast<Uint32>(Key >> 32) };
    const Uint32 V1{ static_cast<Uint32>(Key) };
    const auto It{ State.mEdgeFaceCounts.find(Key) };
    if (It == State.mEdgeFaceCounts.end() || It->second != 2) { return false; }
    if (State.mNonManifoldEdgeCounts[V0] || State.mNonManifoldEdgeCounts[V1]) { return false; }

    const bool BFixedV0{ Geometry.mVertexGroupCounts[V0] > 2 || State.mBoundaryEdgeCounts[V0] ||
        (!Geometry.mMaterialBoundaryVertices.empty() && Geometry.mMaterialBoundaryVertices[V0]) };
    const bool BFixedV1{ Geometry.mVertexGroupCounts[V1] > 2 || State.mBoundaryEdgeCounts[V1] ||
        (!Geometry.mMaterialBoundaryVertices.empty() && Geometry.mMaterialBoundaryVertices[V1]) };
    if (BFixedV0 && BFixedV1) { return false; }

    const FVector3& P0{ Geometry.mPositions[V0] };
    const FVector3& P1{ Geometry.mPositions[V1] };
    FLODQuadric Quadric{ State.mQuadrics[V0] };
    Quadric += State.mQuadrics[V1];
    float Interpolation{};
    if (BFixedV0) { Interpolation = 0.0f; }
    else if (BFixedV1) { Interpolation = 1.0f; }
    else if (Geometry.mVertexGroupCounts[V0] != Geometry.mVertexGroupCounts[V1])
    {
        Interpolation = Geometry.mVertexGroupCounts[V0] > Geometry.mVertexGroupCounts[V1] ? 0.0f : 1.0f;
    }
    else { Interpolation = Quadric.FindEdgeInterpolation(P0, P1, State.mQuadricOrigin); }
    const FVector3 NewPosition{ P0 * (1.0f - Interpolation) + P1 * Interpolation };
    Candidate = { V0, V1, State.mVertexVersions[V0], State.mVertexVersions[V1],
        Quadric.Evaluate(NewPosition, State.mQuadricOrigin), (P1 - P0).LengthSquared(), Interpolation };
    return std::isfinite(Candidate.mCost) && std::isfinite(Candidate.mLengthSquared);
}

bool UMesh::PrepareLODCollapse(const FLODCandidate& Candidate, const FLODGeometry& Geometry, const FLODSimplification& State, FLODCollapse& Collapse) const
{
    FEdge& Edge{ Collapse.mEdge };
    Edge.V0 = Candidate.mV0;
    Edge.V1 = Candidate.mV1;
    Edge.FaceCount = 2;
    Edge.Cost = Candidate.mCost;

    const bool BFixedV0{ Geometry.mVertexGroupCounts[Edge.V0] > 2 || State.mBoundaryEdgeCounts[Edge.V0] ||
        (!Geometry.mMaterialBoundaryVertices.empty() && Geometry.mMaterialBoundaryVertices[Edge.V0]) };
    const bool BFixedV1{ Geometry.mVertexGroupCounts[Edge.V1] > 2 || State.mBoundaryEdgeCounts[Edge.V1] ||
        (!Geometry.mMaterialBoundaryVertices.empty() && Geometry.mMaterialBoundaryVertices[Edge.V1]) };
    if (BFixedV0 && BFixedV1) { return false; }

    // 외곽 / 이음매 교차점은 고정한다. 내부 정점은 경계 쪽으로만 합친다.
    if (BFixedV1 || (!BFixedV0 && Geometry.mVertexGroupCounts[Edge.V1] > Geometry.mVertexGroupCounts[Edge.V0]))
    {
        std::swap(Edge.V0, Edge.V1);
    }
    Collapse.mFixedTarget = BFixedV0 || BFixedV1 || Geometry.mVertexGroupCounts[Edge.V0] != Geometry.mVertexGroupCounts[Edge.V1];
    Edge.NewPosition = Collapse.mFixedTarget ? Geometry.mPositions[Edge.V0] :
        Geometry.mPositions[Candidate.mV0] * (1.0f - Candidate.mInterpolation) + Geometry.mPositions[Candidate.mV1] * Candidate.mInterpolation;
    const float Interpolation{ Edge.V0 == Candidate.mV0 ? Candidate.mInterpolation : 1.0f - Candidate.mInterpolation };

    // 공유 면마다 같은 쪽의 속성 그룹을 짝짓는다. 이음매 반대편의 UV는 섞지 않는다.
    for (Uint32 Corner{ Geometry.mFirstCorners[Edge.V1] }; Corner != UINT32_MAX; Corner = Geometry.mNextCorners[Corner])
    {
        const Uint32 First{ Corner / 3 * 3 };
        for (Uint32 Offset{}; Offset < 3; ++Offset)
        {
            if (Geometry.mIndices[First + Offset] != Edge.V0) { continue; }
            const Uint32 KeepGroup{ Geometry.mRenderGroups[Geometry.mRenderIndices[First + Offset]] };
            const Uint32 RemoveGroup{ Geometry.mRenderGroups[Geometry.mRenderIndices[Corner]] };
            bool BExistingPair{};
            for (Uint32 Index{}; Index < Collapse.mMergeCount; ++Index)
            {
                const auto& Merge{ Collapse.mAttributeMerges[Index] };
                if (Merge.mKeepGroup != KeepGroup && Merge.mRemoveGroup != RemoveGroup) { continue; }
                if (Merge.mKeepGroup != KeepGroup || Merge.mRemoveGroup != RemoveGroup) { return false; }
                BExistingPair = true;
            }
            if (BExistingPair) { continue; }
            if (Collapse.mMergeCount == Collapse.mAttributeMerges.size()) { return false; }
            const FVector2D KeepUV{ Geometry.mAttributeGroups[KeepGroup].mUV };
            const FVector2D RemoveUV{ Geometry.mAttributeGroups[RemoveGroup].mUV };
            Collapse.mAttributeMerges[Collapse.mMergeCount++] = { KeepGroup, RemoveGroup,
                Collapse.mFixedTarget ? KeepUV : KeepUV * (1.0f - Interpolation) + RemoveUV * Interpolation };
        }
    }

    // 짝이 없는 영역을 없애거나 서로 다른 영역을 하나로 합치는 후보는 제외한다.
    if (Collapse.mMergeCount != Geometry.mVertexGroupCounts[Edge.V1] ||
        (!Collapse.mFixedTarget && Collapse.mMergeCount != Geometry.mVertexGroupCounts[Edge.V0])) { return false; }
    if (!Collapse.mFixedTarget && Collapse.mMergeCount == 2)
    {
        const auto& A{ Collapse.mAttributeMerges[0] };
        const auto& B{ Collapse.mAttributeMerges[1] };
        const bool BSameKeepNormal{ FindLODNormalGroup(Geometry, Geometry.mAttributeGroups[A.mKeepGroup].mNormalGroup) ==
            FindLODNormalGroup(Geometry, Geometry.mAttributeGroups[B.mKeepGroup].mNormalGroup) };
        const bool BSameRemoveNormal{ FindLODNormalGroup(Geometry, Geometry.mAttributeGroups[A.mRemoveGroup].mNormalGroup) ==
            FindLODNormalGroup(Geometry, Geometry.mAttributeGroups[B.mRemoveGroup].mNormalGroup) };
        if (BSameKeepNormal != BSameRemoveNormal) { return false; }
    }
    return Collapse.mMergeCount != 0;
}

void UMesh::ApplyLODCollapse(const FLODCollapse& Collapse, FLODGeometry& Geometry, FLODSimplification& State)
{
    const FEdge& Edge{ Collapse.mEdge };
    State.mAffectedTriangles.clear();
    State.mAffectedVertices.clear();
    State.mAffectedEdges.clear();

    for (Uint32 Vertex : { Edge.V0, Edge.V1 })
    {
        for (Uint32 Corner{ Geometry.mFirstCorners[Vertex] }; Corner != UINT32_MAX; Corner = Geometry.mNextCorners[Corner])
        {
            const Uint32 Triangle{ Corner / 3 };
            const Uint32 First{ Triangle * 3 };
            if (Vertex == Edge.V1 && (Geometry.mIndices[First] == Edge.V0 ||
                Geometry.mIndices[First + 1] == Edge.V0 || Geometry.mIndices[First + 2] == Edge.V0)) { continue; }
            State.mAffectedTriangles.push_back(Triangle);
            for (Uint32 Offset{}; Offset < 3; ++Offset) { AddUniqueLODVertex(State.mAffectedVertices, Geometry.mIndices[First + Offset]); }
        }
    }

    Geometry.mPositions[Edge.V0] = Edge.NewPosition;
    // 삭제된 원본 면의 오차도 누적해서 유지한다. 주변 평면을 다시 계산하지 않는다.
    State.mQuadrics[Edge.V0] += State.mQuadrics[Edge.V1];
    for (Uint32 Index{}; Index < Collapse.mMergeCount; ++Index)
    {
        const auto& Merge{ Collapse.mAttributeMerges[Index] };
        Geometry.mAttributeGroups[Merge.mKeepGroup].mUV = Merge.mUV;
        if (!Collapse.mFixedTarget)
        {
            MergeLODNormalGroups(Geometry, Geometry.mAttributeGroups[Merge.mKeepGroup].mNormalGroup,
                Geometry.mAttributeGroups[Merge.mRemoveGroup].mNormalGroup);
        }
    }
    for (Uint32 Vertex : State.mAffectedVertices) { ++State.mVertexVersions[Vertex]; }

    for (Uint32 Triangle : State.mAffectedTriangles)
    {
        const Uint32 First{ Triangle * 3 };
        UpdateLODEdge(State, Geometry.mIndices[First], Geometry.mIndices[First + 1], false);
        UpdateLODEdge(State, Geometry.mIndices[First + 1], Geometry.mIndices[First + 2], false);
        UpdateLODEdge(State, Geometry.mIndices[First + 2], Geometry.mIndices[First], false);
        for (Uint32 Offset{}; Offset < 3; ++Offset) { DetachLODCorner(Geometry, First + Offset); }

        for (Uint32 Offset{}; Offset < 3; ++Offset)
        {
            const Uint32 Corner{ First + Offset };
            if (Geometry.mIndices[Corner] != Edge.V1) { continue; }
            Geometry.mIndices[Corner] = Edge.V0;
            Uint32& Group{ Geometry.mRenderGroups[Geometry.mRenderIndices[Corner]] };
            for (Uint32 Index{}; Index < Collapse.mMergeCount; ++Index)
            {
                const auto& Merge{ Collapse.mAttributeMerges[Index] };
                if (Group == Merge.mRemoveGroup) { Group = Merge.mKeepGroup; break; }
            }
        }

        const Uint32 I0{ Geometry.mIndices[First] };
        const Uint32 I1{ Geometry.mIndices[First + 1] };
        const Uint32 I2{ Geometry.mIndices[First + 2] };
        if (I0 == I1 || I1 == I2 || I2 == I0)
        {
            Geometry.mActiveTriangles[Triangle] = 0;
            --State.mTriangleCount;
            continue;
        }

        for (Uint32 Offset{}; Offset < 3; ++Offset) { AttachLODCorner(Geometry, First + Offset); }
        UpdateLODEdge(State, I0, I1, true);
        UpdateLODEdge(State, I1, I2, true);
        UpdateLODEdge(State, I2, I0, true);
    }

    for (Uint32 Vertex : State.mAffectedVertices) { RefreshLODVertexGroups(Geometry, Vertex, State.mVertexGroups); }

    // 검사를 거절했던 Edge도 주변이 바뀌면 다시 후보가 될 수 있다.
    for (Uint32 Vertex : State.mAffectedVertices)
    {
        for (Uint32 Corner{ Geometry.mFirstCorners[Vertex] }; Corner != UINT32_MAX; Corner = Geometry.mNextCorners[Corner])
        {
            const Uint32 First{ Corner / 3 * 3 };
            for (Uint32 Offset{}; Offset < 3; ++Offset)
            {
                const Uint32 Neighbor{ Geometry.mIndices[First + Offset] };
                if (Neighbor != Vertex) { State.mAffectedEdges.push_back(MakeLODEdgeKey(Vertex, Neighbor)); }
            }
        }
    }
    std::ranges::sort(State.mAffectedEdges);
    const auto DuplicateEdges{ std::ranges::unique(State.mAffectedEdges) };
    State.mAffectedEdges.erase(DuplicateEdges.begin(), DuplicateEdges.end());
    for (Uint64 Key : State.mAffectedEdges)
    {
        FLODCandidate Candidate{};
        if (!MakeLODCandidate(Key, Geometry, State, Candidate)) { continue; }
        State.mCandidates.push_back(Candidate);
        std::ranges::push_heap(State.mCandidates, CompareLODCandidates);
    }
}

UMesh::FLODRenderData UMesh::BuildLODRenderData(const FLODGeometry& Geometry) const
{
    const auto SourcePositions{ GetVertexAttributeData<EVertexAttribute::Position>() };
    const auto SourceNormals{ GetVertexAttributeData<EVertexAttribute::Normal>() };
    const auto SourceColors{ GetVertexAttributeData<EVertexAttribute::Color>() };
    FLODRenderData RenderData{};
    RenderData.mPositions.assign(SourcePositions.begin(), SourcePositions.end());
    RenderData.mNormals.assign(SourceNormals.begin(), SourceNormals.end());
    RenderData.mUVs.assign(Geometry.mSourceUVs.begin(), Geometry.mSourceUVs.end());
    RenderData.mIndices = Geometry.mRenderIndices;

    TArray<FVector3> NormalSums(Geometry.mNormalParents.size());
    TArray<Uint32> RenderNormalGroups(SourceNormals.size(), UINT32_MAX);

    for (std::size_t Index{}; Index + 2 < Geometry.mIndices.size(); Index += 3)
    {
        FVector3 FaceNormal{};
        if (!SourceNormals.empty())
        {
            const FVector3& P0{ Geometry.mPositions[Geometry.mIndices[Index]] };
            const FVector3& P1{ Geometry.mPositions[Geometry.mIndices[Index + 1]] };
            const FVector3& P2{ Geometry.mPositions[Geometry.mIndices[Index + 2]] };
            // 정규화 전 외적을 더하면 면적이 큰 삼각형에 더 큰 가중치가 부여된다.
            FaceNormal = (P1 - P0).Cross(P2 - P0);
        }

        for (Uint32 Corner{}; Corner < 3; ++Corner)
        {
            const Uint32 GeometryIndex{ Geometry.mIndices[Index + Corner] };
            const Uint32 RenderIndex{ Geometry.mRenderIndices[Index + Corner] };
            const FLODAttributeGroup& Attribute{ Geometry.mAttributeGroups[Geometry.mRenderGroups[RenderIndex]] };
            RenderData.mPositions[RenderIndex] = Geometry.mPositions[GeometryIndex];
            if (!RenderData.mUVs.empty()) { RenderData.mUVs[RenderIndex] = Attribute.mUV; }
            if (!SourceNormals.empty())
            {
                const Uint32 Group{ FindLODNormalGroup(Geometry, Attribute.mNormalGroup) };
                NormalSums[Group] += FaceNormal;
                RenderNormalGroups[RenderIndex] = Group;
            }
        }
    }

    for (FVector3& Normal : NormalSums) { Normal.Normalize(); }
    for (std::size_t RenderIndex{}; RenderIndex < RenderNormalGroups.size(); ++RenderIndex)
    {
        const Uint32 Group{ RenderNormalGroups[RenderIndex] };
        if (Group != UINT32_MAX && NormalSums[Group].LengthSquared() > 0.0f)
        {
            RenderData.mNormals[RenderIndex] = NormalSums[Group];
        }
    }

    // 같은 속성 그룹은 인덱스도 공유해 정점 캐시를 재사용한다. 색상이 다르면 기존 정점을 유지한다.
    TArray<Uint32> GroupRenderIndices(Geometry.mAttributeGroups.size(), UINT32_MAX);
    for (Uint32& RenderIndex : RenderData.mIndices)
    {
        Uint32& Representative{ GroupRenderIndices[Geometry.mRenderGroups[RenderIndex]] };
        if (Representative == UINT32_MAX) { Representative = RenderIndex; }
        if (!SourceColors.empty())
        {
            const FColor4& A{ SourceColors[RenderIndex] };
            const FColor4& B{ SourceColors[Representative] };
            if (A.X != B.X || A.Y != B.Y || A.Z != B.Z || A.W != B.W) { continue; }
        }
        RenderIndex = Representative;
    }

    if (!Geometry.mTriangleSubMeshes.empty())
    {
        // 생성 마지막에 한 번만 재질별로 모은다. 모든 구간은 같은 GPU 인덱스 버퍼를 쓴다.
        TArray<Uint32> WritePositions{};
        for (const FSubMesh& SubMesh : Geometry.mSubMeshes) { WritePositions.push_back(SubMesh.mFirstIndex); }
        TArray<Uint32> SectionIndices(RenderData.mIndices.size());
        for (std::size_t Triangle{}; Triangle < Geometry.mTriangleSubMeshes.size(); ++Triangle)
        {
            Uint32& Destination{ WritePositions[Geometry.mTriangleSubMeshes[Triangle]] };
            for (Uint32 Corner{}; Corner < 3; ++Corner) { SectionIndices[Destination++] = RenderData.mIndices[Triangle * 3 + Corner]; }
        }
        RenderData.mIndices.swap(SectionIndices);
    }

    return RenderData;
}

TArray<FEdge> UMesh::BuildEdges(const TArray<Uint32>& Indices)
{
    TArray<FEdge> Edges{};
    std::unordered_map<uint64_t, std::size_t> EdgeMap{};
    Edges.reserve(Indices.size() / 2);
    EdgeMap.reserve(Indices.size() / 2);

    auto AddEdge = [&](Uint32 A, Uint32 B)
    {
        if (A > B) std::swap(A, B);

        const uint64_t Key{ (static_cast<uint64_t>(A) << 32) | static_cast<uint64_t>(B)};

        const auto [It, BInserted]{ EdgeMap.try_emplace(Key, Edges.size()) };

        if (BInserted)
        {
            FEdge Edge{};
            Edge.V0 = A;
            Edge.V1 = B;
            Edge.FaceCount = 1;

            Edges.push_back(Edge);
        }
        else
        {
            ++Edges[It->second].FaceCount;
        }
    };

    for (std::size_t Index{}; Index + 2 < Indices.size(); Index += 3)
    {
        const Uint32 V0{ Indices[Index] };
        const Uint32 V1{ Indices[Index + 1] };
        const Uint32 V2{ Indices[Index + 2] };

        AddEdge(V0, V1);
        AddEdge(V1, V2);
        AddEdge(V2, V0);
    }

    return Edges;
}

FEdge UMesh::FindShortestEdge(const TArray<FEdge>& Edges, const TArray<FVector3>& Positions)
{
    FEdge ShortestEdge;
    float MinLengthSq = FLT_MAX;

	for (const FEdge& Edge : Edges)
	{
        const FVector3& V0 = Positions[Edge.V0];
        const FVector3& V1 = Positions[Edge.V1];

		float LengthSq = (V1 - V0).LengthSquared();

		if (LengthSq < MinLengthSq)
		{
			MinLengthSq = LengthSq;
			ShortestEdge = Edge;
		}
	}

    ShortestEdge.Cost = MinLengthSq;
    return ShortestEdge;
}

bool UMesh::CanCollapseEdge(const FLODCollapse& Collapse, const FLODGeometry& Geometry, FLODSimplification& State) const
{
    constexpr float Epsilon{ 1e-8f };
    constexpr float MinUVAreaRatio{ 1e-4f };
    const FEdge& Edge{ Collapse.mEdge };
    const bool BFixedTarget{ Collapse.mFixedTarget };

    if (Edge.FaceCount != 2) { return false; }

    TArray<Uint32>& NeighborsV0{ State.mNeighborsV0 };
    TArray<Uint32>& NeighborsV1{ State.mNeighborsV1 };
    TArray<Uint32>& OppositeVertices{ State.mOppositeVertices };
    NeighborsV0.clear();
    NeighborsV1.clear();
    OppositeVertices.clear();
    State.mRemainingFaces.clear();

    const bool BHasUVs{ !Geometry.mSourceUVs.empty() };
    const auto GetUV{ [&Geometry](Uint32 Corner)
    {
        return Geometry.mAttributeGroups[Geometry.mRenderGroups[Geometry.mRenderIndices[Corner]]].mUV;
    } };
    const auto GetCollapsedUV{ [&Geometry, &Collapse](Uint32 Corner)
    {
        const Uint32 Group{ Geometry.mRenderGroups[Geometry.mRenderIndices[Corner]] };
        for (Uint32 Index{}; Index < Collapse.mMergeCount; ++Index)
        {
            const auto& Merge{ Collapse.mAttributeMerges[Index] };
            if (Group == Merge.mKeepGroup || Group == Merge.mRemoveGroup) { return Merge.mUV; }
        }
        return Geometry.mAttributeGroups[Group].mUV;
    } };
    const auto UVArea{ [](const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
    } };

    // 전체 메시 대신 두 끝점에 연결된 삼각형만 방문한다.
    for (Uint32 Vertex : { Edge.V0, Edge.V1 })
    {
        for (Uint32 Corner{ Geometry.mFirstCorners[Vertex] }; Corner != UINT32_MAX; Corner = Geometry.mNextCorners[Corner])
        {
            const Uint32 Index{ Corner / 3 * 3 };
            const Uint32 I0{ Geometry.mIndices[Index] };
            const Uint32 I1{ Geometry.mIndices[Index + 1] };
            const Uint32 I2{ Geometry.mIndices[Index + 2] };
            const bool BHasV0{ I0 == Edge.V0 || I1 == Edge.V0 || I2 == Edge.V0 };
            const bool BHasV1{ I0 == Edge.V1 || I1 == Edge.V1 || I2 == Edge.V1 };

            // 공유 삼각형은 V0 목록에서 이미 처리했다.
            if (Vertex == Edge.V1 && BHasV0) { continue; }

            for (Uint32 Neighbor : { I0, I1, I2 })
            {
                if (Neighbor == Edge.V0 || Neighbor == Edge.V1) { continue; }
                if (BHasV0) { AddUniqueLODVertex(NeighborsV0, Neighbor); }
                if (BHasV1) { AddUniqueLODVertex(NeighborsV1, Neighbor); }
                if (BHasV0 && BHasV1) { AddUniqueLODVertex(OppositeVertices, Neighbor); }
            }

            // 공유 삼각형은 제거되므로 남는 인접 삼각형만 검사한다.
            if (BHasV0 && BHasV1) { continue; }

            // Collapse 후 같은 세 정점을 가진 면이 겹치는 경우도 제외한다.
            const Uint32 A{ I0 == Edge.V0 || I0 == Edge.V1 ? I1 : I0 };
            const Uint32 B{ I2 == Edge.V0 || I2 == Edge.V1 ? I1 : I2 };
            const Uint64 FaceKey{ MakeLODEdgeKey(A, B) };
            if (std::ranges::find(State.mRemainingFaces, FaceKey) != State.mRemainingFaces.end()) { return false; }
            State.mRemainingFaces.push_back(FaceKey);

            // 경계 쪽에만 붙은 면은 Position과 UV가 그대로이므로 재검사가 필요 없다.
            if (BFixedTarget && BHasV0) { continue; }

            FVector3 P0{ Geometry.mPositions[I0] };
            FVector3 P1{ Geometry.mPositions[I1] };
            FVector3 P2{ Geometry.mPositions[I2] };
            const FVector3 OldNormal{ (P1 - P0).Cross(P2 - P0) };

            if (I0 == Edge.V0 || I0 == Edge.V1) { P0 = Edge.NewPosition; }
            if (I1 == Edge.V0 || I1 == Edge.V1) { P1 = Edge.NewPosition; }
            if (I2 == Edge.V0 || I2 == Edge.V1) { P2 = Edge.NewPosition; }

            const FVector3 NewNormal{ (P1 - P0).Cross(P2 - P0) };
            if (NewNormal.LengthSquared() <= Epsilon) { return false; }
            if (OldNormal.Dot(NewNormal) <= 0.0f) { return false; }

            // UV 보간으로 텍스처 삼각형이 뒤집히거나 납작해지는 후보를 제외한다.
            if (BHasUVs)
            {
                FVector2D UV0{ GetUV(Index) };
                FVector2D UV1{ GetUV(Index + 1) };
                FVector2D UV2{ GetUV(Index + 2) };
                const float OldArea{ UVArea(UV0, UV1, UV2) };
                UV0 = GetCollapsedUV(Index);
                UV1 = GetCollapsedUV(Index + 1);
                UV2 = GetCollapsedUV(Index + 2);
                const float NewArea{ UVArea(UV0, UV1, UV2) };

                // 원래 UV 면적이 0인 면(단색 UV 등)은 방향 검사에서 제외한다.
                if (OldArea != 0.0f && ((OldArea > 0.0f) != (NewArea > 0.0f) ||
                    std::abs(NewArea) <= std::abs(OldArea) * MinUVAreaRatio)) { return false; }
            }
        }
    }

    // 내부 Edge는 정확히 두 개의 삼각형과 반대 정점을 가져야 한다.
    if (OppositeVertices.size() != 2) { return false; }

    // 두 정점이 추가 이웃을 공유하면 Collapse 후 연결 구조가 겹친다.
    for (Uint32 Neighbor : NeighborsV0)
    {
        if (std::ranges::find(NeighborsV1, Neighbor) != NeighborsV1.end() &&
            std::ranges::find(OppositeVertices, Neighbor) == OppositeVertices.end())
        {
            return false;
        }
    }

    return true;
}

bool UMesh::CreateLODBuffers(ID3D11Device* Device, const FLODRenderData& RenderData, FGeneratedLOD& LOD)
{
    if (Device == nullptr || RenderData.mPositions.empty() || RenderData.mIndices.empty()) { return false; }

    const auto CreateBuffer{ [Device](const auto& Data, UINT BindFlags, Microsoft::WRL::ComPtr<ID3D11Buffer>& Buffer)
    {
        const std::size_t ByteSize{ Data.size() * sizeof(Data[0]) };
        if (ByteSize == 0 || ByteSize > std::numeric_limits<UINT>::max()) { return false; }

        D3D11_BUFFER_DESC BufferDesc{};
        BufferDesc.ByteWidth = static_cast<UINT>(ByteSize);
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.BindFlags = BindFlags;
        D3D11_SUBRESOURCE_DATA InitialData{};
        InitialData.pSysMem = Data.data();
        return SUCCEEDED(Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf()));
    } };

    if (!CreateBuffer(RenderData.mPositions, D3D11_BIND_VERTEX_BUFFER, LOD.mVertexBuffer)) { return false; }
    if (!RenderData.mNormals.empty() && !CreateBuffer(RenderData.mNormals, D3D11_BIND_VERTEX_BUFFER, LOD.mNormalBuffer)) { return false; }
    if (!RenderData.mUVs.empty() && !CreateBuffer(RenderData.mUVs, D3D11_BIND_VERTEX_BUFFER, LOD.mUVBuffer)) { return false; }
    if (!CreateBuffer(RenderData.mIndices, D3D11_BIND_INDEX_BUFFER, LOD.mIndexBuffer)) { return false; }
    LOD.mIndexCount = static_cast<Uint32>(RenderData.mIndices.size());
    return true;
}

void UMesh::Serialize(FArchive& Ar) {
    UAsset::Serialize(Ar);
}

bool UMesh::CreateIndexBuffer(ID3D11Device* Device, const std::span<const Uint32>& InIndices) {
    if (Device == nullptr || InIndices.empty()) {
        return false;
    }

    const std::size_t ByteSize{InIndices.size_bytes()};

    if (ByteSize > std::numeric_limits<UINT>::max()) {
        return false;
    }

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth = static_cast<UINT>(ByteSize);
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;
    BufferDesc.MiscFlags = 0;
    BufferDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = InIndices.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer{};

    const HRESULT Result{Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf())};

    if (FAILED(Result)) {
        return false;
    }

    mIndexBuffer = std::move(Buffer);

    mIndices.assign(InIndices.begin(), InIndices.end());

    return true;
}

void UMesh::Reset() {
    RaycastAccelerationStructure = FMeshRaycastAccelerationStructure{};

    for (Microsoft::WRL::ComPtr<ID3D11Buffer>& Buffer : mVertexBuffers) {
        Buffer.Reset();
    }

    for (std::unique_ptr<FVertexAttributeStorageBase>& Storage : mAttributeStorage) {
        Storage.reset();
    }

    mIndexBuffer.Reset();
    mIndices.clear();
    mSubMeshes.clear();

    for (FGeneratedLOD& LOD : mGeneratedLODs) {
        LOD.Reset();
    }

    mGeneratedLODs.clear();
}

const TArray<Uint32>& UMesh::GetIndices() const {
    return mIndices;
}

const TArray<UMesh::FSubMesh>& UMesh::GetSubMeshes(int Level) const {
    const FGeneratedLOD* LOD{ GetGeneratedLOD(Level) };
    if (LOD && LOD->IsValid()) { return LOD->mSubMeshes; }
    return mSubMeshes;
}

void UMesh::SetSubMeshes(const std::span<FSubMesh>& InSubMeshes) {
    mSubMeshes.assign(InSubMeshes.begin(), InSubMeshes.end());
    // 구간 / 재질이 바뀌면 이전 구간을 참조하는 LOD를 다시 생성해야 한다.
    mGeneratedLODs.clear();
    ++mRenderRevision;
}

bool UMesh::Raycast(const FRay& Ray, float& OutDistance, float MaxDistance) const {
    return RaycastAccelerationStructure.Raycast(*this, Ray, OutDistance, MaxDistance);
}

bool FMeshRaycastAccelerationStructure::BuildStructure(UMesh& Mesh) {
    Nodes.clear();
    mIndexGroups.clear();
    const auto Positions{ Mesh.GetVertexAttributeData<EVertexAttribute::Position>() };
    const TArray<Uint32>& Indices{ Mesh.GetIndices() };
    if (Positions.empty() || Indices.empty() || Indices.size() % 3 != 0 || Indices.size() / 3 > std::numeric_limits<Uint32>::max()) return false;
    for (Uint32 Index : Indices) {
        if (Index >= Positions.size()) return false;
    }
    TArray<DirectX::BoundingBox> TriangleBounds;
    MinMaxBox Bounds;

    TriangleBounds.resize(Indices.size() / 3);
    mIndexGroups.resize(TriangleBounds.size());

    for (std::size_t t = 0; t < TriangleBounds.size(); ++t)
    {
        const DirectX::XMVECTOR V0 = Positions[Indices[t * 3 + 0]].ToSimpleMath();
        const DirectX::XMVECTOR V1 = Positions[Indices[t * 3 + 1]].ToSimpleMath();
        const DirectX::XMVECTOR V2 = Positions[Indices[t * 3 + 2]].ToSimpleMath();

        const auto Min = DirectX::XMVectorMin(V0, DirectX::XMVectorMin(V1, V2));
        const auto Max = DirectX::XMVectorMax(V0, DirectX::XMVectorMax(V1, V2));

        DirectX::BoundingBox::CreateFromPoints(TriangleBounds[t], Min, Max);
        Bounds.Expand(TriangleBounds[t]);
        mIndexGroups[t] = static_cast<TrisIndex>(t);
    }
    MakeChild(TriangleBounds, 0, static_cast<Uint32>(mIndexGroups.size()), Bounds);
    return true;
}

Uint32 FMeshRaycastAccelerationStructure::MakeChild(const TArray<DirectX::BoundingBox>& TriangleBounds, Uint32 First, Uint32 Count, const MinMaxBox& Bounds) {
    FNode Node{};
    Node.BoundingBox.Center = { Bounds.minX * 0.5f + Bounds.maxX * 0.5f, Bounds.minY * 0.5f + Bounds.maxY * 0.5f, Bounds.minZ * 0.5f + Bounds.maxZ * 0.5f };
    Node.BoundingBox.Extents = { Bounds.maxX * 0.5f - Bounds.minX * 0.5f, Bounds.maxY * 0.5f - Bounds.minY * 0.5f, Bounds.maxZ * 0.5f - Bounds.minZ * 0.5f };
    Node.BoundingBox.Extents.x += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.x) + Node.BoundingBox.Extents.x);
    Node.BoundingBox.Extents.y += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.y) + Node.BoundingBox.Extents.y);
    Node.BoundingBox.Extents.z += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.z) + Node.BoundingBox.Extents.z);
    const auto& BoundingBox = Node.BoundingBox;

    MinMaxBox Bin[3][Slice];
    Uint32 BinCounts[3][Slice]{};
    const float Centers[3]{ BoundingBox.Center.x, BoundingBox.Center.y, BoundingBox.Center.z };
    const float Extents[3]{ BoundingBox.Extents.x, BoundingBox.Extents.y, BoundingBox.Extents.z };
    const auto GetBinIndex = [&](const DirectX::BoundingBox& Box, Uint32 Axis) {
        const float BoxCenters[3]{ Box.Center.x, Box.Center.y, Box.Center.z };
        const float Normalized = Extents[Axis] > 0.0f ? (BoxCenters[Axis] - Centers[Axis]) / Extents[Axis] : -1.0f;
        return static_cast<Uint32>(std::clamp((Normalized + 1.0f) * Slice / 2, 0.0f, static_cast<float>(Slice - 1)));
    };
    for (Uint32 Offset = 0; Offset < Count; ++Offset) {
        const auto& Box = TriangleBounds[mIndexGroups[First + Offset]];
        for (Uint32 Axis = 0; Axis < 3; ++Axis) {
            const Uint32 Index = GetBinIndex(Box, Axis);
            Bin[Axis][Index].Expand(Box);
            ++BinCounts[Axis][Index];
        }
    }
    Uint32 BestAxis = 0; // x = 0, y = 1, z = 2
    Uint32 BestLeftEnd = 0;
    float BestCost = std::numeric_limits<float>::max();
    MinMaxBox BestLeftBox{};
    MinMaxBox BestRightBox{};

    for (Uint32 Axis = 0; Axis < 3; ++Axis) {
        MinMaxBox RightBoxes[Slice];
        Uint32 RightTrisCounts[Slice];
        RightBoxes[Slice - 1] = Bin[Axis][Slice - 1];
        RightTrisCounts[Slice - 1] = BinCounts[Axis][Slice - 1];
        for (int j = Slice - 2; j >= 0; --j) {
            RightBoxes[j] = MinMaxBox::Merge(Bin[Axis][j], RightBoxes[j + 1]);
            RightTrisCounts[j] = BinCounts[Axis][j] + RightTrisCounts[j + 1];
        }
        MinMaxBox LeftBox;
        Uint32 LeftTrisCount = 0;
        for (int LeftEnd = 0; LeftEnd < Slice - 1; ++LeftEnd) {
            LeftBox = MinMaxBox::Merge(LeftBox, Bin[Axis][LeftEnd]);
            LeftTrisCount += BinCounts[Axis][LeftEnd];
            const MinMaxBox& RightBox = RightBoxes[LeftEnd + 1];
            const Uint32 RightTrisCount = RightTrisCounts[LeftEnd + 1];
            if (LeftTrisCount == 0 || RightTrisCount == 0) continue;
            float Cost = LeftTrisCount * LeftBox.SurfaceArea() + RightTrisCount * RightBox.SurfaceArea();
            if (Cost < BestCost) {
                BestCost = Cost;
                BestLeftEnd = LeftEnd;
                BestAxis = Axis;
                BestLeftBox = LeftBox;
                BestRightBox = RightBox;
            }
        }
    }

    int retIndex = Nodes.size();
    Nodes.push_back(Node);

    float leafCost = static_cast<float>(Count);
    float Area = Bounds.SurfaceArea();
    float splitCost = Area > 0.0f && BestCost < std::numeric_limits<float>::max() ? TraversalCostOverInternalCost + BestCost / Area : std::numeric_limits<float>::max();
    if (ForceSingleTriangleLeaf ? Count > 1 : splitCost < leafCost) {
        Uint32 LeftCount = Count / 2;
        if (BestCost < std::numeric_limits<float>::max()) {
            auto Begin = mIndexGroups.begin() + First;
            auto Middle = std::partition(Begin, Begin + Count, [&](TrisIndex Index) { return GetBinIndex(TriangleBounds[Index], BestAxis) <= BestLeftEnd; });
            LeftCount = static_cast<Uint32>(Middle - Begin);
        }
        if (BestCost == std::numeric_limits<float>::max() || LeftCount == 0 || LeftCount == Count) {
            LeftCount = Count / 2;
            BestLeftBox = {};
            BestRightBox = {};
            for (Uint32 Offset = 0; Offset < Count; ++Offset) {
                if (Offset < LeftCount) BestLeftBox.Expand(TriangleBounds[mIndexGroups[First + Offset]]);
                else BestRightBox.Expand(TriangleBounds[mIndexGroups[First + Offset]]);
            }
        }
        Nodes[retIndex].mLeft = MakeChild(TriangleBounds, First, LeftCount, BestLeftBox);
        Nodes[retIndex].mRight = MakeChild(TriangleBounds, First + LeftCount, Count - LeftCount, BestRightBox);
    }
    else {
        Nodes[retIndex].mIndexStart = First;
        Nodes[retIndex].mIndexCount = Count;
    }

    return retIndex;
}

bool FMeshRaycastAccelerationStructure::Raycast(const UMesh& Mesh, const FRay& Ray, float& OutDistance, float MaxDistance) const {
    if (Nodes.empty() || !(MaxDistance >= 0.0f)) return false;
    const auto Positions{ Mesh.GetVertexAttributeData<EVertexAttribute::Position>() };
    const TArray<Uint32>& Indices{ Mesh.GetIndices() };
    float ClosestDistance = MaxDistance;
    const DirectX::XMVECTOR Origin = Ray.position;
    const DirectX::XMVECTOR Direction = Ray.direction;
    const DirectX::XMVECTOR IsParallel = DirectX::XMVectorLessOrEqual(DirectX::XMVectorAbs(Direction), DirectX::g_RayEpsilon);
    const DirectX::XMVECTOR InverseDirection = DirectX::XMVectorReciprocal(DirectX::XMVectorSelect(Direction, DirectX::XMVectorSplatOne(), IsParallel));
    const auto IntersectsBox = [&](const DirectX::BoundingBox& Box, float& EntryDistance) {
        const DirectX::XMVECTOR Extents = DirectX::XMLoadFloat3(&Box.Extents);
        const DirectX::XMVECTOR Offset = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&Box.Center), Origin);
        const DirectX::XMVECTOR Outside = DirectX::XMVectorAndInt(IsParallel, DirectX::XMVectorGreater(DirectX::XMVectorAbs(Offset), Extents));
        if (!DirectX::XMVector3EqualInt(Outside, DirectX::XMVectorZero())) return false;
        const DirectX::XMVECTOR T1 = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(Offset, Extents), InverseDirection);
        const DirectX::XMVECTOR T2 = DirectX::XMVectorMultiply(DirectX::XMVectorAdd(Offset, Extents), InverseDirection);
        DirectX::XMVECTOR Near = DirectX::XMVectorSelect(DirectX::XMVectorMin(T1, T2), DirectX::g_FltMin, IsParallel);
        DirectX::XMVECTOR Far = DirectX::XMVectorSelect(DirectX::XMVectorMax(T1, T2), DirectX::g_FltMax, IsParallel);
        Near = DirectX::XMVectorMax(Near, DirectX::XMVectorSplatY(Near));
        Near = DirectX::XMVectorMax(Near, DirectX::XMVectorSplatZ(Near));
        Far = DirectX::XMVectorMin(Far, DirectX::XMVectorSplatY(Far));
        Far = DirectX::XMVectorMin(Far, DirectX::XMVectorSplatZ(Far));
        EntryDistance = (std::max)(DirectX::XMVectorGetX(Near), 0.0f);
        return EntryDistance <= DirectX::XMVectorGetX(Far) && EntryDistance <= ClosestDistance;
    };
    float RootDistance = 0.0f;
    if (!IntersectsBox(Nodes[0].BoundingBox, RootDistance)) return false;
    struct FStackEntry {
        Uint32 NodeIndex;
        float EntryDistance;
    };
    FStackEntry Stack[64];
    Uint32 StackSize = 0;
    TArray<FStackEntry> Overflow;
    const auto Push = [&](FStackEntry Entry) {
        if (StackSize < std::size(Stack)) Stack[StackSize++] = Entry;
        else Overflow.push_back(Entry);
    };
    FStackEntry Entry{ 0, RootDistance };
    bool BHit = false;
    for (;;) {
        if (Entry.EntryDistance <= ClosestDistance) {
            const FNode& Node = Nodes[Entry.NodeIndex];
            if (Node.mIndexCount == 0) {
                FStackEntry Children[2]{ { Node.mLeft, 0.0f }, { Node.mRight, 0.0f } };
                bool Hits[2]{};
                for (Uint32 i = 0; i < 2; ++i) {
                    Hits[i] = IntersectsBox(Nodes[Children[i].NodeIndex].BoundingBox, Children[i].EntryDistance);
                }
                if (Hits[0] && Hits[1]) {
                    if (Children[0].EntryDistance > Children[1].EntryDistance) std::swap(Children[0], Children[1]);
                    Push(Children[1]);
                    Entry = Children[0];
                    continue;
                }
                if (Hits[0] || Hits[1]) { Entry = Children[Hits[0] ? 0 : 1]; continue; }
            }
            else for (Uint32 i = 0; i < Node.mIndexCount; ++i) {
                const std::size_t Index = static_cast<std::size_t>(mIndexGroups[Node.mIndexStart + i]) * 3;
                const DirectX::XMVECTOR V0 = Positions[Indices[Index]].ToSimpleMath();
                const DirectX::XMVECTOR V1 = Positions[Indices[Index + 1]].ToSimpleMath();
                const DirectX::XMVECTOR V2 = Positions[Indices[Index + 2]].ToSimpleMath();
                float Distance = 0.0f;
                if (DirectX::TriangleTests::Intersects(Origin, Direction, V0, V1, V2, Distance) && Distance <= ClosestDistance) {
                    ClosestDistance = Distance;
                    BHit = true;
                }
            }
        }
        if (!Overflow.empty()) { Entry = Overflow.back(); Overflow.pop_back(); }
        else if (StackSize > 0) Entry = Stack[--StackSize];
        else break;
    }
    if (BHit) OutDistance = ClosestDistance;
    return BHit;
}
