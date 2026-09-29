#include "UMesh.h"
#include "pch.h"
#include "UMesh.h"

#include "Core/Console/Console.h"
#include "Core/Base/FGuid.h"

#include "FObjImporter.h"
#include "Asset/Importer/FObjSerializer.h"
#include <ranges>
#include <windows.h>

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

    if (Attribute == EVertexAttribute::Position && LOD && LOD->IsValid())
        return LOD->mVertexBuffer.Get();

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
        return static_cast<Uint32>(LOD->mIndices.size());

    return static_cast<Uint32>((std::min)(mIndices.size(), static_cast<std::size_t>(UINT32_MAX)));
}

bool UMesh::HasLOD(int Level) const
{
    if (Level == 0)
        return mIndexBuffer && !mIndices.empty();

    const FGeneratedLOD* LOD{ GetGeneratedLOD(Level) };
    return LOD && LOD->IsValid();
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

    if (Level == 0)
        return false;

    if (mGeneratedLODs.size() < Level)
        mGeneratedLODs.resize(Level);

    FGeneratedLOD& OutputLOD{ mGeneratedLODs[Level - 1] };
    OutputLOD.Reset();

    const auto PositionData{ GetVertexAttributeData<EVertexAttribute::Position>() };

	// Pos, Indices를 복사
    TArray<FVector3> LODPositions(PositionData.begin(), PositionData.end());
    TArray<Uint32> LODIndices{mIndices};

    const Uint32 OriginalTriangleCount{static_cast<Uint32>(LODIndices.size() / 3)};
    const Uint32 TargetTriangleCount{(std::max)(1u, static_cast<Uint32>(OriginalTriangleCount * TargetRatio))};

    while (LODIndices.size() / 3 > TargetTriangleCount) 
    {
        TArray<FEdge> Edges{BuildEdges(LODIndices)};
        if (Edges.empty()) { break; }

        std::unordered_set<Uint32> ProtectedVertices{};

        for (const FEdge& Edge : Edges)
        {
            if (Edge.FaceCount != 2)
            {
                ProtectedVertices.insert(Edge.V0);
                ProtectedVertices.insert(Edge.V1);
            }
        }

        for (FEdge& Edge : Edges) {
            Edge.NewPosition = (LODPositions[Edge.V0] + LODPositions[Edge.V1]) * 0.5f;
            Edge.Cost = (LODPositions[Edge.V1] - LODPositions[Edge.V0]).LengthSquared();
        }

        const auto CompareCost{[](const FEdge& Left, const FEdge& Right) { return Left.Cost > Right.Cost; }};
        std::ranges::make_heap(Edges, CompareCost);

		// 가장 짧은 Edge를 찾고 Collapse 가능 여부 확인(무한 루프 방지)
        FEdge SelectedEdge{};
        bool BFoundCollapsibleEdge{false};
        while (!Edges.empty()) {
            std::ranges::pop_heap(Edges, CompareCost);
            FEdge Candidate{Edges.back()};
            Edges.pop_back();

            if (Candidate.FaceCount != 2) { continue; }

            if (CanCollapseEdge(Candidate, LODPositions, LODIndices)) {
                SelectedEdge = Candidate;
                BFoundCollapsibleEdge = true;
                break;
            }
        }

        // 더 줄일 수 없다.
        if (!BFoundCollapsibleEdge) { break; }

		// Collapse Edge
        LODPositions[SelectedEdge.V0] = SelectedEdge.NewPosition;
        for (Uint32& Index : LODIndices) {
            if (Index == SelectedEdge.V1) {
                Index = SelectedEdge.V0;
            }
        }

        // Degenerate Triangle 제거
        TArray<Uint32> CollapsedIndices{};
        CollapsedIndices.reserve(LODIndices.size());
        for (std::size_t Index{}; Index + 2 < LODIndices.size(); Index += 3) {
            const Uint32 I0{LODIndices[Index]};
            const Uint32 I1{LODIndices[Index + 1]};
            const Uint32 I2{LODIndices[Index + 2]};

            if (I0 == I1 || I1 == I2 || I2 == I0) { continue; }

            CollapsedIndices.push_back(I0);
            CollapsedIndices.push_back(I1);
            CollapsedIndices.push_back(I2);
        }

        LODIndices = std::move(CollapsedIndices);
    }

    OutputLOD.mPositions = std::move(LODPositions);
    OutputLOD.mIndices = std::move(LODIndices);

    if (!CreateLODVertexBuffer(Device, OutputLOD) || !CreateLODIndexBuffer(Device, OutputLOD))
    {
        OutputLOD.Reset();
        return false;
    }

    return true;
}

TArray<FEdge> UMesh::BuildEdges(const TArray<Uint32>& Indices)
{
    TArray<FEdge> Edges{};
    std::unordered_map<uint64_t, std::size_t> EdgeMap{};

    auto AddEdge = [&](Uint32 A, Uint32 B)
    {
        if (A > B) std::swap(A, B);

        const uint64_t Key{ (static_cast<uint64_t>(A) << 32) | static_cast<uint64_t>(B)};

        const auto It{ EdgeMap.find(Key) };

        if (It == EdgeMap.end())
        {
            const std::size_t EdgeIndex{ Edges.size() };
            EdgeMap.emplace(Key, EdgeIndex);

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

bool UMesh::CanCollapseEdge(const FEdge& Edge, TArray<FVector3>& LODPositions, TArray<Uint32>& LODIndices)
{
	constexpr float Epsilon = 1e-8f;
    if (Edge.FaceCount != 2) { return false; }

	for (size_t i = 0; i + 2 < LODIndices.size(); i += 3)
	{
        Uint32 I0 = LODIndices[i];
        Uint32 I1 = LODIndices[i + 1];
        Uint32 I2 = LODIndices[i + 2];

        bool bHasV0 = I0 == Edge.V0 || I1 == Edge.V0 || I2 == Edge.V0;
        bool bHasV1 = I0 == Edge.V1 || I1 == Edge.V1 || I2 == Edge.V1;

        if (!bHasV0 && !bHasV1) continue;
        if (bHasV0 && bHasV1) continue; // 어차피 제거될 Triangle

        FVector3 P0 = LODPositions[I0];
        FVector3 P1 = LODPositions[I1];
        FVector3 P2 = LODPositions[I2];

		FVector3 OldNormal = (P1 - P0).Cross(P2 - P0);

        if (I0 == Edge.V0 || I0 == Edge.V1) P0 = Edge.NewPosition;
        if (I1 == Edge.V0 || I1 == Edge.V1) P1 = Edge.NewPosition;
        if (I2 == Edge.V0 || I2 == Edge.V1) P2 = Edge.NewPosition;

        FVector3 NewNormal = (P1 - P0).Cross(P2 - P0);

        if (NewNormal.Dot(NewNormal) <= Epsilon)
            return false;

        if (OldNormal.Dot(NewNormal) <= 0.0f)
            return false;
	}

    return true;
}

bool UMesh::CreateLODVertexBuffer(ID3D11Device* Device, FGeneratedLOD& LOD)
{
    if (Device == nullptr || LOD.mPositions.empty())
        return false;

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth =
        static_cast<UINT>(LOD.mPositions.size() * sizeof(FVector3));

    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = LOD.mPositions.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;

    HRESULT Result = Device->CreateBuffer(
        &BufferDesc,
        &InitialData,
        Buffer.GetAddressOf()
    );

    if (FAILED(Result))
        return false;

    LOD.mVertexBuffer = std::move(Buffer);

    return true;
}

bool UMesh::CreateLODIndexBuffer(ID3D11Device* Device, FGeneratedLOD& LOD)
{
    if (Device == nullptr || LOD.mIndices.empty())
        return false;

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth =
        static_cast<UINT>(LOD.mIndices.size() * sizeof(Uint32));

    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = LOD.mIndices.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;

    HRESULT Result = Device->CreateBuffer(
        &BufferDesc,
        &InitialData,
        Buffer.GetAddressOf()
    );

    if (FAILED(Result))
        return false;

    LOD.mIndexBuffer = std::move(Buffer);

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
    RaycastAccelerationStructure = FRaycastAccelerationStructure{};
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

const TArray<UMesh::FSubMesh>& UMesh::GetSubMeshes() const {
    return mSubMeshes;
}

void UMesh::SetSubMeshes(const std::span<FSubMesh>& InSubMeshes) {
    mSubMeshes.assign(InSubMeshes.begin(), InSubMeshes.end());
}

bool UMesh::Raycast(const FRay& Ray, float& OutDistance) const {
    return RaycastAccelerationStructure.Raycast(*this, Ray, OutDistance);
}

bool FRaycastAccelerationStructure::BuildStructure(UMesh& Mesh) {
    Nodes.clear();
    mIndexGroups.clear();
    const auto Positions{ Mesh.GetVertexAttributeData<EVertexAttribute::Position>() };
    const TArray<Uint32>& Indices{ Mesh.GetIndices() };
    if (Positions.empty() || Indices.empty() || Indices.size() % 3 != 0 || Indices.size() / 3 > std::numeric_limits<Uint32>::max()) return false;
    for (Uint32 Index : Indices) {
        if (Index >= Positions.size()) return false;
    }
    TArray<DirectX::BoundingBox> TriangleBounds;
    TArray<TrisIndex> SubTrisArray;
    MinMaxBox Bounds;

    TriangleBounds.resize(Indices.size() / 3);
    SubTrisArray.resize(TriangleBounds.size());
    mIndexGroups.reserve(Indices.size() / 3);

    for (std::size_t t = 0; t < TriangleBounds.size(); ++t)
    {
        const DirectX::XMVECTOR V0 = Positions[Indices[t * 3 + 0]].ToSimpleMath();
        const DirectX::XMVECTOR V1 = Positions[Indices[t * 3 + 1]].ToSimpleMath();
        const DirectX::XMVECTOR V2 = Positions[Indices[t * 3 + 2]].ToSimpleMath();

        const auto Min = DirectX::XMVectorMin(V0, DirectX::XMVectorMin(V1, V2));
        const auto Max = DirectX::XMVectorMax(V0, DirectX::XMVectorMax(V1, V2));

        DirectX::BoundingBox::CreateFromPoints(TriangleBounds[t], Min, Max);
        Bounds.Expand(TriangleBounds[t]);
        SubTrisArray[t] = static_cast<TrisIndex>(t);
    }
    MakeChild(TriangleBounds, SubTrisArray, Bounds);
    return true;
}

Uint32 FRaycastAccelerationStructure::MakeChild(const TArray<DirectX::BoundingBox>& TriangleBounds, const TArray<TrisIndex>& SubTrisArray, const MinMaxBox& Bounds) {
    FNode Node{};
    Node.BoundingBox.Center = { Bounds.minX * 0.5f + Bounds.maxX * 0.5f, Bounds.minY * 0.5f + Bounds.maxY * 0.5f, Bounds.minZ * 0.5f + Bounds.maxZ * 0.5f };
    Node.BoundingBox.Extents = { Bounds.maxX * 0.5f - Bounds.minX * 0.5f, Bounds.maxY * 0.5f - Bounds.minY * 0.5f, Bounds.maxZ * 0.5f - Bounds.minZ * 0.5f };
    Node.BoundingBox.Extents.x += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.x) + Node.BoundingBox.Extents.x);
    Node.BoundingBox.Extents.y += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.y) + Node.BoundingBox.Extents.y);
    Node.BoundingBox.Extents.z += 4.0f * std::numeric_limits<float>::epsilon() * (std::abs(Node.BoundingBox.Center.z) + Node.BoundingBox.Extents.z);
    const auto& BoundingBox = Node.BoundingBox;

    MinMaxBox Bin[3][Slice];
    TArray<TrisIndex> TrisBin[3][Slice];

    for (TrisIndex i : SubTrisArray) {
        const auto& Box = TriangleBounds[i];

        const float RelativeCenter[3]{
            Box.Center.x - BoundingBox.Center.x,
            Box.Center.y - BoundingBox.Center.y,
            Box.Center.z - BoundingBox.Center.z
        };

        const float Extents[3]{
            BoundingBox.Extents.x,
            BoundingBox.Extents.y,
            BoundingBox.Extents.z
        };

        for (Uint32 Axis = 0; Axis < 3; ++Axis) {
            float Normalized = Extents[Axis] > 0.0f ? RelativeCenter[Axis] / Extents[Axis] : -1.0f;

            Uint32 Index = static_cast<Uint32>(std::clamp((Normalized + 1.0f) * Slice / 2, 0.0f, static_cast<float>(Slice - 1)));

            Bin[Axis][Index].Expand(Box);
            TrisBin[Axis][Index].push_back(i);
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
        RightTrisCounts[Slice - 1] = static_cast<Uint32>(TrisBin[Axis][Slice - 1].size());
        for (int j = Slice - 2; j >= 0; --j) {
            RightBoxes[j] = MinMaxBox::Merge(Bin[Axis][j], RightBoxes[j + 1]);
            RightTrisCounts[j] = static_cast<Uint32>(TrisBin[Axis][j].size()) + RightTrisCounts[j + 1];
        }
        MinMaxBox LeftBox;
        Uint32 LeftTrisCount = 0;
        for (int LeftEnd = 0; LeftEnd < Slice - 1; ++LeftEnd) {
            LeftBox = MinMaxBox::Merge(LeftBox, Bin[Axis][LeftEnd]);
            LeftTrisCount += static_cast<Uint32>(TrisBin[Axis][LeftEnd].size());
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

    float leafCost = static_cast<float>(SubTrisArray.size());
    float Area = Bounds.SurfaceArea();
    float splitCost = Area > 0.0f && BestCost < std::numeric_limits<float>::max() ? TraversalCostOverInternalCost + BestCost / Area : std::numeric_limits<float>::max();
    if (splitCost < leafCost) {
        auto LeftRange = std::span{ TrisBin[BestAxis] }.first(BestLeftEnd + 1) | std::views::join;
        TArray<TrisIndex> LeftSubTrisArray{ LeftRange.begin(), LeftRange.end() };
        auto RightRange = std::span{ TrisBin[BestAxis] }.subspan(BestLeftEnd + 1) | std::views::join;
        TArray<TrisIndex> RightSubTrisArray(RightRange.begin(), RightRange.end());
        Nodes[retIndex].mLeft = MakeChild(TriangleBounds, LeftSubTrisArray, BestLeftBox);
        Nodes[retIndex].mRight = MakeChild(TriangleBounds, RightSubTrisArray, BestRightBox);
    }
    else {
        Nodes[retIndex].mIndexStart = static_cast<Uint32>(mIndexGroups.size());
        Nodes[retIndex].mIndexCount = static_cast<Uint32>(SubTrisArray.size());
        mIndexGroups.insert(mIndexGroups.end(), SubTrisArray.begin(), SubTrisArray.end());
    }

    return retIndex;
}

bool FRaycastAccelerationStructure::Raycast(const UMesh& Mesh, const FRay& Ray, float& OutDistance) const {
    if (Nodes.empty()) return false;
    const auto Positions{ Mesh.GetVertexAttributeData<EVertexAttribute::Position>() };
    const TArray<Uint32>& Indices{ Mesh.GetIndices() };
    float RootDistance = 0.0f;
    if (!Nodes[0].BoundingBox.Intersects(Ray.position, Ray.direction, RootDistance)) return false;
    struct FStackEntry {
        Uint32 NodeIndex;
        float EntryDistance;
    };
    TArray<FStackEntry> Stack;
    Stack.push_back({ 0, (std::max)(RootDistance, 0.0f) });
    float ClosestDistance = std::numeric_limits<float>::max();
    bool BHit = false;
    while (!Stack.empty()) {
        const FStackEntry Entry = Stack.back();
        Stack.pop_back();
        if (Entry.EntryDistance > ClosestDistance) continue;
        const FNode& Node = Nodes[Entry.NodeIndex];
        if (Node.mIndexCount == 0) {
            FStackEntry Children[2]{ { Node.mLeft, 0.0f }, { Node.mRight, 0.0f } };
            bool Hits[2]{};
            for (Uint32 i = 0; i < 2; ++i) {
                Hits[i] = Nodes[Children[i].NodeIndex].BoundingBox.Intersects(Ray.position, Ray.direction, Children[i].EntryDistance);
                Children[i].EntryDistance = (std::max)(Children[i].EntryDistance, 0.0f);
                Hits[i] = Hits[i] && Children[i].EntryDistance <= ClosestDistance;
            }
            if (Hits[0] && Hits[1]) {
                if (Children[0].EntryDistance < Children[1].EntryDistance) std::swap(Children[0], Children[1]);
                Stack.push_back(Children[0]);
                Stack.push_back(Children[1]);
            }
            else if (Hits[0]) Stack.push_back(Children[0]);
            else if (Hits[1]) Stack.push_back(Children[1]);
            continue;
        }
        for (Uint32 i = 0; i < Node.mIndexCount; ++i) {
            const std::size_t Index = static_cast<std::size_t>(mIndexGroups[Node.mIndexStart + i]) * 3;
            const DirectX::XMVECTOR V0 = Positions[Indices[Index]].ToSimpleMath();
            const DirectX::XMVECTOR V1 = Positions[Indices[Index + 1]].ToSimpleMath();
            const DirectX::XMVECTOR V2 = Positions[Indices[Index + 2]].ToSimpleMath();
            float Distance = 0.0f;
            if (DirectX::TriangleTests::Intersects(Ray.position, Ray.direction, V0, V1, V2, Distance) && Distance < ClosestDistance) {
                ClosestDistance = Distance;
                BHit = true;
            }
        }
    }
    if (BHit) OutDistance = ClosestDistance;
    return BHit;
}
