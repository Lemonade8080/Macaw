#pragma once

#include "Core/Base/UObject.h"
#include "Asset/Pipeline/Defines.h"
#include "Core/Base/FVertexAttribute.h"
#include "Core/Spatial/FBVH8.h"
#include "Core/Spatial/FBVH8TrianglePackets.h"

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "UAsset.h"
#include "Core/Base/FAssetHandle.h"
#include "Core/Base/TypeInfo.h"

class UMesh;
struct alignas(64) FMeshPickingSource {
    const UMesh* Mesh = nullptr;
    const BVH8::FNode* Nodes = nullptr;
    const BVH8::FTrianglePacket* Packets = nullptr;
    Uint32 RootReference = 0;
    bool Raycast(const FRay& Ray, float& OutDistance, float MaxDistance, bool ReverseWinding) const;
};

class FMeshRaycastAccelerationStructure {
    friend class UMesh;
private:
    static constexpr Uint32 Slice = 32;
    static constexpr Uint32 MaxTrianglesPerPacket = 8;

    using TrisIndex = Uint32;
    struct FNode {
        DirectX::BoundingBox BoundingBox;
        Uint32 mLeft = std::numeric_limits<Uint32>::max();
        Uint32 mRight = std::numeric_limits<Uint32>::max();
        Uint32 mIndexStart = 0;
        Uint32 mIndexCount = 0;
    };
    struct MinMaxBox {
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        float maxZ = std::numeric_limits<float>::lowest();

        inline MinMaxBox() = default;

        inline MinMaxBox(const DirectX::BoundingBox& Box)
            : minX(Box.Center.x - Box.Extents.x)
            , minY(Box.Center.y - Box.Extents.y)
            , minZ(Box.Center.z - Box.Extents.z)
            , maxX(Box.Center.x + Box.Extents.x)
            , maxY(Box.Center.y + Box.Extents.y)
            , maxZ(Box.Center.z + Box.Extents.z)
        {
        }

        inline void Expand(const DirectX::BoundingBox& Box) {
            if (minX > Box.Center.x - Box.Extents.x)
                minX = Box.Center.x - Box.Extents.x;
            if (minY > Box.Center.y - Box.Extents.y)
                minY = Box.Center.y - Box.Extents.y;
            if (minZ > Box.Center.z - Box.Extents.z)
                minZ = Box.Center.z - Box.Extents.z;

            if (maxX < Box.Center.x + Box.Extents.x)
                maxX = Box.Center.x + Box.Extents.x;
            if (maxY < Box.Center.y + Box.Extents.y)
                maxY = Box.Center.y + Box.Extents.y;
            if (maxZ < Box.Center.z + Box.Extents.z)
                maxZ = Box.Center.z + Box.Extents.z;
        }

        inline float SurfaceArea() const {
            const float dx = maxX - minX;
            const float dy = maxY - minY;
            const float dz = maxZ - minZ;

            if (dx < 0.0f || dy < 0.0f || dz < 0.0f)
                return 0.0f;

            return 2.0f * (dx * dy + dy * dz + dz * dx);
        }

        static MinMaxBox Merge(const MinMaxBox& Box1, const MinMaxBox& Box2) {
            MinMaxBox Result;
            Result.minX = (std::min)(Box1.minX, Box2.minX);
            Result.minY = (std::min)(Box1.minY, Box2.minY);
            Result.minZ = (std::min)(Box1.minZ, Box2.minZ);

            Result.maxX = (std::max)(Box1.maxX, Box2.maxX);
            Result.maxY = (std::max)(Box1.maxY, Box2.maxY);
            Result.maxZ = (std::max)(Box1.maxZ, Box2.maxZ);
            return Result;
        }
    };
public:
    bool BuildStructure(UMesh& Mesh);
    bool Raycast(const FRay& Ray, float& OutDistance, float MaxDistance = std::numeric_limits<float>::max(), bool ReverseWinding = false) const;
private:
    Uint32 MakeChild(TArray<FNode>& BuildNodes, const TArray<DirectX::BoundingBox>& TriangleBounds, Uint32 First, Uint32 Count, const MinMaxBox& Bounds);
    TArray<Uint32> mIndexGroups;
    TArray<BVH8::FTrianglePacket> mTrianglePackets;
    FBVH8 mTree;
};

/* LOD */
struct FEdge
{
    Uint32 V0;
    Uint32 V1;
    Uint32 FaceCount{ 0 };

    double Cost{ 0.0 };
    FVector3 NewPosition;
};

class UMesh : public UAsset {
private:
    struct FVertexAttributeStorageBase { virtual ~FVertexAttributeStorageBase() = default; virtual const void* GetData() const = 0; virtual Uint32 GetCount() const = 0; virtual Uint32 GetStride() const = 0; };

    template <typename T> struct TVertexAttributeStorage final : FVertexAttributeStorageBase { explicit TVertexAttributeStorage(std::span<const T> InData); const void* GetData() const override; Uint32 GetCount() const override; Uint32 GetStride() const override; std::vector<T> mData{}; };

public:
    struct FSubMesh {
        Uint32 mFirstIndex{0};
        Uint32 mIndexCount{0};
        Uint32 mMaterialGroupIndex{0};
        // 생성된 LOD의 통계용 원본 개수. 원본 SubMesh에서는 mIndexCount를 사용한다.
        Uint32 mSourceIndexCount{0};
    };

    using FMaterialResolver = std::function<FAssetHandle(const std::filesystem::path& MaterialPath)>;
    using FMaterialGroupResolver = std::function<std::optional<Uint32>(FAssetHandle MaterialHandle, const FString& GroupName)>;

public:
    UMesh() = default;
    ~UMesh() override { mPickingSource->Mesh = nullptr; }
    std::shared_ptr<const FMeshPickingSource> GetPickingSource() const { return mPickingSource; }
    bool RebuildPickingStructure();

    UMesh(const UMesh&) = delete;
    UMesh& operator=(const UMesh&) = delete;

    UMesh(UMesh&&) noexcept = default;
    UMesh& operator=(UMesh&&) noexcept = default;

public:
    JG_DECLARE_DERIVED_TYPEINFO(UMesh, UAsset);

    bool Initialize(ID3D11Device* Device, const std::filesystem::path& SourceObjPath, const std::filesystem::path& BinaryPath, const FMaterialResolver& MaterialResolver, const FMaterialGroupResolver& MaterialGroupResolver, bool FlipUV);

    template <CVertexAttributeView... TAttributes> bool Make(ID3D11Device* Device, const std::span<const Uint32>& InIndices, const TAttributes&... InAttributes);

    ID3D11Buffer* GetVertexBuffer(EVertexAttribute Attribute) const;
    ID3D11Buffer* GetVertexBuffer(EVertexAttribute Attribute, int Level) const;
    ID3D11Buffer* GetIndexBuffer() const;
    ID3D11Buffer* GetIndexBuffer(int Level) const;
    Uint32 GetIndexCount(int Level = 0) const;
    bool HasLOD(int Level) const;

    Uint64 GetRenderRevision() const;

    bool HasVertexAttribute(EVertexAttribute Attribute) const;

    Uint32 GetVertexStride(EVertexAttribute Attribute) const;
    Uint32 GetVertexAttributeCount(EVertexAttribute Attribute) const;

    const void* GetVertexData(EVertexAttribute Attribute) const;

    template <EVertexAttribute Attribute> std::span<const TVertexAttributeElementType<Attribute>> GetVertexAttributeData() const;

    const TArray<Uint32>& GetIndices() const;

    const TArray<FSubMesh>& GetSubMeshes(int Level = 0) const;

    void SetSubMeshes(const std::span<FSubMesh>& InSubMeshes);

    bool Raycast(const FRay& Ray, float& OutDistance, float MaxDistance = std::numeric_limits<float>::max(), bool ReverseWinding = false) const;

    const inline DirectX::BoundingOrientedBox GetBoundingBox() const { return mBoundingBox; }

    /* LOD */
    bool GenerateLOD(ID3D11Device* Device, Uint32 Level, float TargetRatio);

    TArray<FEdge> BuildEdges(const TArray<Uint32>& Indices);
	FEdge FindShortestEdge(const TArray<FEdge>& Edges, const TArray<FVector3>& Positions);

private:
    // 대칭 4x4 Quadric의 상삼각 성분만 저장한다. 생성 중 원본 평면 오차를 누적한다.
    struct FLODQuadric
    {
        std::array<double, 10> mValues{};

        void AddPlane(double X, double Y, double Z, double D);
        FLODQuadric& operator+=(const FLODQuadric& Other);
        double Evaluate(const FVector3& Position, const FVector3& Origin) const;
        float FindEdgeInterpolation(const FVector3& P0, const FVector3& P1, const FVector3& Origin) const;
    };

    struct FLODAttributeGroup
    {
        FVector2D mUV{};
        Uint32 mNormalGroup{ UINT32_MAX };
    };

    // Collapse 계산에만 사용하는 임시 데이터. 렌더 정점의 속성 경계는 별도로 유지한다.
    struct FLODGeometry
    {
        TArray<FVector3> mPositions{};
        TArray<Uint32> mIndices{};
        TArray<Uint32> mRenderIndices{};
        TArray<FSubMesh> mSubMeshes{};
        // 재질이 하나일 때는 삼각형별 구간과 경계 배열을 만들지 않는다.
        TArray<Uint32> mTriangleSubMeshes{};
        TArray<Uint8> mMaterialBoundaryVertices{};

        // 같은 기하 위치에도 UV / Normal이 다른 렌더 그룹을 따로 유지한다.
        TArray<FLODAttributeGroup> mAttributeGroups{};
        TArray<Uint32> mRenderGroups{};
        TArray<Uint32> mVertexGroupCounts{};
        // UV만 갈라진 면은 Normal을 함께 계산하고, 원본의 각진 경계는 분리한다.
        TArray<Uint32> mNormalParents{};
        TArray<Uint8> mNormalRanks{};

        // Corner 연결을 양방향으로 유지해 영향을 받은 면만 분리 / 재연결한다.
        TArray<Uint32> mFirstCorners{};
        TArray<Uint32> mNextCorners{};
        TArray<Uint32> mPreviousCorners{};
        TArray<Uint8> mActiveTriangles{};
        std::span<const FVector2D> mSourceUVs{};
    };

    struct FLODCandidate
    {
        Uint32 mV0{};
        Uint32 mV1{};
        Uint32 mVersionV0{};
        Uint32 mVersionV1{};
        double mCost{};
        float mLengthSquared{};
        float mInterpolation{ 0.5f };
    };

    struct FLODCollapse
    {
        struct FAttributeMerge
        {
            Uint32 mKeepGroup{};
            Uint32 mRemoveGroup{};
            FVector2D mUV{};
        };

        // V0는 남길 정점, V1은 제거할 정점이다.
        FEdge mEdge{};
        // 공유 면 두 개에서 찾은 대응 관계. 이음매 양쪽을 서로 섞지 않는다.
        std::array<FAttributeMerge, 2> mAttributeMerges{};
        Uint32 mMergeCount{};
        bool mFixedTarget{};
    };

    struct FLODSimplification
    {
        std::unordered_map<Uint64, Uint32> mEdgeFaceCounts{};
        TArray<Uint32> mBoundaryEdgeCounts{};
        TArray<Uint32> mNonManifoldEdgeCounts{};
        TArray<Uint32> mVertexVersions{};
        TArray<FLODCandidate> mCandidates{};
        TArray<FLODQuadric> mQuadrics{};
        FVector3 mQuadricOrigin{};
        Uint32 mTriangleCount{};

        // 매 후보 / Collapse마다 할당하지 않고 작업 공간을 재사용한다.
        TArray<Uint32> mAffectedTriangles{};
        TArray<Uint32> mAffectedVertices{};
        TArray<Uint64> mAffectedEdges{};
        TArray<Uint32> mNeighborsV0{};
        TArray<Uint32> mNeighborsV1{};
        TArray<Uint32> mOppositeVertices{};
        TArray<Uint64> mRemainingFaces{};
        TArray<Uint32> mVertexGroups{};
    };

    struct FLODRenderData
    {
        TArray<FVector3> mPositions{};
        TArray<FVector3> mNormals{};
        TArray<FVector2D> mUVs{};
        TArray<Uint32> mIndices{};
    };

    struct FGeneratedLOD
    {
        Uint32 mIndexCount{};
        TArray<FSubMesh> mSubMeshes{};

        Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer{};
        Microsoft::WRL::ComPtr<ID3D11Buffer> mNormalBuffer{};
        Microsoft::WRL::ComPtr<ID3D11Buffer> mUVBuffer{};
        Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer{};

        bool IsValid() const
        {
            return mVertexBuffer != nullptr && mIndexBuffer != nullptr && mIndexCount != 0;
        }

        void Reset()
        {
            mVertexBuffer.Reset();
            mNormalBuffer.Reset();
            mUVBuffer.Reset();
            mIndexBuffer.Reset();
            mIndexCount = 0;
            mSubMeshes.clear();
        }
    };

    FGeneratedLOD* GetGeneratedLOD(int Level);
    const FGeneratedLOD* GetGeneratedLOD(int Level) const;

    bool BuildLODGeometry(FLODGeometry& Geometry) const;
    bool BuildLODSubMeshes(FLODGeometry& Geometry) const;
    void SimplifyLODGeometry(FLODGeometry& Geometry, Uint32 TargetTriangleCount);
    static bool CompareLODCandidates(const FLODCandidate& Left, const FLODCandidate& Right);
    static void UpdateLODEdge(FLODSimplification& State, Uint32 V0, Uint32 V1, bool BAddFace);
    static void DetachLODCorner(FLODGeometry& Geometry, Uint32 Corner);
    static void AttachLODCorner(FLODGeometry& Geometry, Uint32 Corner);
    static void RefreshLODVertexGroups(FLODGeometry& Geometry, Uint32 Vertex, TArray<Uint32>& Groups);
    static Uint32 FindLODNormalGroup(const FLODGeometry& Geometry, Uint32 Group);
    static void MergeLODNormalGroups(FLODGeometry& Geometry, Uint32 Group0, Uint32 Group1);
    bool MakeLODCandidate(Uint64 Key, const FLODGeometry& Geometry, const FLODSimplification& State, FLODCandidate& Candidate) const;
    bool PrepareLODCollapse(const FLODCandidate& Candidate, const FLODGeometry& Geometry, const FLODSimplification& State, FLODCollapse& Collapse) const;
    bool CanCollapseEdge(const FLODCollapse& Collapse, const FLODGeometry& Geometry, FLODSimplification& State) const;
    void ApplyLODCollapse(const FLODCollapse& Collapse, FLODGeometry& Geometry, FLODSimplification& State);
    FLODRenderData BuildLODRenderData(const FLODGeometry& Geometry) const;
    bool CreateLODBuffers(ID3D11Device* Device, const FLODRenderData& RenderData, FGeneratedLOD& LOD);

    // Level 1은 index 0, Level 2는 index 1
    TArray<FGeneratedLOD> mGeneratedLODs{};

protected:
    virtual void Serialize(FArchive& Ar) override;

private:
    template <typename... TAttributes> static consteval bool AreVertexAttributesUnique();

    template <CVertexAttributeView TAttribute> bool CreateVertexBuffer(ID3D11Device* Device, const TAttribute& InAttribute);

    bool CreateIndexBuffer(ID3D11Device* Device, const std::span<const Uint32>& InIndices);

    void Reset();

    static std::size_t GetAttributeIndex(EVertexAttribute Attribute);

    static std::size_t GetAttributeCount();

    bool BuildBoundingBoxFromMesh();

private:
    TFixedArray<Microsoft::WRL::ComPtr<ID3D11Buffer>, static_cast<std::size_t>(EVertexAttribute::MAX)> mVertexBuffers{};
    TFixedArray<std::unique_ptr<FVertexAttributeStorageBase>, static_cast<std::size_t>(EVertexAttribute::MAX)> mAttributeStorage{};

    Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer{nullptr};

    TArray<Uint32> mIndices{};

    TArray<FSubMesh> mSubMeshes{};
    Uint64 mRenderRevision{1};

    DirectX::BoundingOrientedBox mBoundingBox{ DirectX::XMFLOAT3{0.f, 0.f, 0.f}, DirectX::XMFLOAT3{0.f, 0.f, 0.f}, DirectX::XMFLOAT4{0.f, 0.f, 0.f, 1.f} };

    std::shared_ptr<FMeshPickingSource> mPickingSource = std::make_shared<FMeshPickingSource>(this);
    FMeshRaycastAccelerationStructure RaycastAccelerationStructure{};
};

template <typename T> UMesh::TVertexAttributeStorage<T>::TVertexAttributeStorage(std::span<const T> InData)
    : mData(InData.begin(), InData.end()) {
}

template <typename T> const void* UMesh::TVertexAttributeStorage<T>::GetData() const {
    return mData.data();
}

template <typename T> Uint32 UMesh::TVertexAttributeStorage<T>::GetCount() const {
    return static_cast<Uint32>(mData.size());
}

template <typename T> Uint32 UMesh::TVertexAttributeStorage<T>::GetStride() const {
    return static_cast<Uint32>(sizeof(T));
}

template <CVertexAttributeView... TAttributes> bool UMesh::Make(ID3D11Device* Device, const std::span<const Uint32>& InIndices, const TAttributes&... InAttributes) {
    static_assert(sizeof...(TAttributes) > 0, "UMesh requires at least one vertex attribute.");
    static_assert(AreVertexAttributesUnique<TAttributes...>(), "Duplicate vertex attributes are not allowed.");

    Reset();

    if (Device == nullptr || InIndices.empty()) {
        return false;
    }

    Uint32 ExpectedVertexCount{0};
    bool BFirstAttribute{true};
    bool BSuccess{true};

    auto ProcessAttribute{[&](const auto& InAttribute) {
        if (!BSuccess) {
            return;
        }

        if (InAttribute.mData.empty() || InAttribute.mData.size() > std::numeric_limits<Uint32>::max()) {
            BSuccess = false;
            return;
        }

        const Uint32 AttributeVertexCount{static_cast<Uint32>(InAttribute.mData.size())};

        if (BFirstAttribute) {
            ExpectedVertexCount = AttributeVertexCount;
            BFirstAttribute = false;
        } else if (AttributeVertexCount != ExpectedVertexCount) {
            BSuccess = false;
            return;
        }

        if (!CreateVertexBuffer(Device, InAttribute)) {
            BSuccess = false;
        }
    }};

    (ProcessAttribute(InAttributes), ...);

    if (!BSuccess) {
        Reset();
        return false;
    }

    if (!CreateIndexBuffer(Device, InIndices)) {
        Reset();
        return false;
    }

    mIndices.assign(InIndices.begin(), InIndices.end());

    return true;
}

template <EVertexAttribute Attribute> std::span<const TVertexAttributeElementType<Attribute>> UMesh::GetVertexAttributeData() const {
    using ElementType = TVertexAttributeElementType<Attribute>;
    using StorageType = TVertexAttributeStorage<ElementType>;

    constexpr std::size_t Index{static_cast<std::size_t>(Attribute)};

    if (!mAttributeStorage[Index]) {
        return {};
    }

    const StorageType* Storage{static_cast<const StorageType*>(mAttributeStorage[Index].get())};

    return std::span<const ElementType>{Storage->mData.data(), Storage->mData.size()};
}

template <typename... TAttributes> consteval bool UMesh::AreVertexAttributesUnique() {
    constexpr std::array<EVertexAttribute, sizeof...(TAttributes)> Attributes{std::remove_cvref_t<TAttributes>::AttributeType...};

    for (std::size_t I{0}; I < Attributes.size(); ++I) {
        for (std::size_t J{I + 1}; J < Attributes.size(); ++J) {
            if (Attributes[I] == Attributes[J]) {
                return false;
            }
        }
    }

    return true;
}

template <CVertexAttributeView TAttribute> bool UMesh::CreateVertexBuffer(ID3D11Device* Device, const TAttribute& InAttribute) {
    using AttributeType = std::remove_cvref_t<TAttribute>;
    using ElementType = typename AttributeType::ElementType;

    constexpr EVertexAttribute Attribute{AttributeType::AttributeType};
    constexpr std::size_t AttributeIndex{static_cast<std::size_t>(Attribute)};

    if (InAttribute.mData.empty() || InAttribute.mData.size_bytes() > std::numeric_limits<UINT>::max()) {
        return false;
    }

    D3D11_BUFFER_DESC BufferDesc{};
    BufferDesc.ByteWidth = static_cast<UINT>(InAttribute.mData.size_bytes());
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;
    BufferDesc.MiscFlags = 0;
    BufferDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = InAttribute.mData.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer{};

    const HRESULT Result{Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf())};

    if (FAILED(Result)) {
        return false;
    }

    mVertexBuffers[AttributeIndex] = std::move(Buffer);
    mAttributeStorage[AttributeIndex] = std::make_unique<TVertexAttributeStorage<ElementType>>(InAttribute.mData);

    return true;
}
