#include "UMesh.h"
#include "pch.h"
#include "UMesh.h"

#include "Core/Console/Console.h"

#include "FObjImporter.h"
#include "Asset/Importer/FObjSerializer.h"

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
    const bool BLoadedFromBinary{BHasBinary && FObjSerializer::LoadBinary(BinaryPath.string().c_str(), Geometry)};

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

        if (!ObjImporter.LoadObjFile(SourceObjPath.string().c_str(), Geometry, FlipUV)) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[UMesh] Failed to import OBJ geometry: %s", SourceObjPath.generic_string().c_str());
            Console::AddLog(Console::STDOutHandle, ELogLevel::Error, ELogCategory::Etc, "[UMesh] Import Failed. Check Obj File Path : %s", SourceObjPath.generic_string().c_str());
            return false;
        }

        if (!BinaryPath.empty() && !FObjSerializer::SaveBinary(Geometry, BinaryPath.string().c_str())) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "[UMesh] Failed to create model binary: %s", BinaryPath.generic_string().c_str());
        } else if (!BinaryPath.empty()) {
            Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "[UMesh] Created model binary: %s", BinaryPath.generic_string().c_str());
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
