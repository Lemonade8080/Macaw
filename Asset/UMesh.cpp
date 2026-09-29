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

ID3D11Buffer* UMesh::GetIndexBuffer() const {
    return mIndexBuffer.Get();
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
