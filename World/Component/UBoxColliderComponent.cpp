#include "pch.h"
#include "Core/Render/ILineDrawContext.h"
#include <cfloat>
#include "Core/Property/IPropertyEditorContext.h"
#include "UBoxColliderComponent.h"

#include "UMeshComponent.h"
#include "World/AActor.h"
#include "Asset/UMesh.h"
#include "Core/Base/UObjectSystem.h"

#include <array>

void UBoxColliderComponent::SetMeshComponent(UMeshComponent* InMeshComponent) {
    mMeshComponent.Set(InMeshComponent);
    mPendingMeshComponentGuid = {};
    BuildBoundsFromMesh();
}

UMeshComponent* UBoxColliderComponent::GetMeshComponent() const {
    return mMeshComponent.Get();
}

bool UBoxColliderComponent::BuildBoundsFromMesh() {
    UMeshComponent* Mesh{mMeshComponent.Get()};
    const UMesh* Asset{Mesh != nullptr ? Mesh->ResolveMesh() : nullptr};
    if (Asset == nullptr) {
        return false;
    }

    const auto Positions{Asset->GetVertexAttributeData<EVertexAttribute::Position>()};
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
    DirectX::BoundingOrientedBox::CreateFromBoundingBox(mObb, Bounds);
    SetPickingBox(mObb);
    return true;
}

bool UBoxColliderComponent::RaycastBounds(const FRay& Ray, float& OutDistance) const {
    DirectX::BoundingOrientedBox WorldBox{};
    mObb.Transform(WorldBox, GetComponentToWorld().ToSimpleMath());
    return WorldBox.Intersects(Ray.position, Ray.direction, OutDistance);
}

FVector3 UBoxColliderComponent::GetExtent() const {
    return FVector3{mObb.Extents.x, mObb.Extents.y, mObb.Extents.z};
}

void UBoxColliderComponent::SetExtent(const FVector3& InExtent) {
    mObb.Extents = DirectX::XMFLOAT3(InExtent.mX, InExtent.mY, InExtent.mZ);
    SetPickingBox(mObb);
}

bool UBoxColliderComponent::ResolveLoadedReferences() {
    if (!UCollisionComponent::ResolveLoadedReferences()) {
        return false;
    }
    if (mPendingMeshComponentGuid.IsValid()) {
        UObject* Object{UObjectSystem::Resolve(UObjectSystem::FindHandleByGuid(mPendingMeshComponentGuid))};
        if (Object == nullptr || !Object->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo())) {
            return false;
        }
        mMeshComponent.Set(static_cast<UMeshComponent*>(Object));
        mPendingMeshComponentGuid = {};
    }
    BuildBoundsFromMesh();
    return true;
}

void UBoxColliderComponent::InitializeComponent() {
    UCollisionComponent::InitializeComponent();
    BuildBoundsFromMesh();
}

void UBoxColliderComponent::Serialize(FArchive& Archive) {
    UCollisionComponent::Serialize(Archive);

    FString MeshComponentGuid{};
    if (UMeshComponent * Mesh{mMeshComponent.Get()}) {
        MeshComponentGuid = Mesh->GetGuid().ToString();
    }
    Archive.Serialize("GuidMeshComponent", MeshComponentGuid);
    if (Archive.IsLoading() && !MeshComponentGuid.empty() && !mPendingMeshComponentGuid.Parse(MeshComponentGuid)) {
        mPendingMeshComponentGuid = {};
    }

    FVector3 Center{mObb.Center};
    FVector3 Extent{mObb.Extents};
    FQuat Orientation{mObb.Orientation};
    Archive.Serialize("OBB_Center", Center);
    Archive.Serialize("OBB_Extent", Extent);
    Archive.Serialize("OBB_Orientation", Orientation);
    if (Archive.IsLoading()) {
        mObb.Center = Center.ToSimpleMath();
        mObb.Extents = Extent.ToSimpleMath();
        mObb.Orientation = Orientation.ToSimpleMath();
        SetPickingBox(mObb);
    }
}

void UBoxColliderComponent::DrawPanels(IPropertyEditorContext* Context) {
    UCollisionComponent::DrawPanels(Context);
    Context->DrawVector3("Extent", GetExtent(), 0.05f, 0.001f, FLT_MAX, [this](const FVector3& Extent) {
        SetExtent(Extent);
    });

    AActor* Actor{GetOwner()};
    if (Actor == nullptr) {
        return;
    }
    UMeshComponent* CurrentMesh{GetMeshComponent()};
    const char* Preview{CurrentMesh != nullptr ? CurrentMesh->GetTypeInfo()->mTypeName.data() : "None"};
    std::vector<FPropertyReferenceOption> Candidates{};
    for (const std::unique_ptr<UActorComponent>& Candidate : Actor->GetComponents()) {
        UActorComponent* CandidateComponent{Candidate.get()};
        if (CandidateComponent == nullptr || !CandidateComponent->GetTypeInfo()->IsA<UMeshComponent>()) {
            continue;
        }

        auto* Mesh{static_cast<UMeshComponent*>(CandidateComponent)};
        Candidates.push_back({Mesh, FString{Mesh->GetTypeInfo()->mTypeName}, Mesh == CurrentMesh, [this, Mesh] {
                                  SetMeshComponent(Mesh);
                              }});
    }
    Context->DrawReferencePicker("Source Mesh Component", Preview, CurrentMesh == nullptr, [this] {
        SetMeshComponent(nullptr);
    }, Candidates);
    Context->DrawButton("Build Bounds From Mesh", [this] {
        BuildBoundsFromMesh();
    });
}

void UBoxColliderComponent::DrawEditorBounds(ILineDrawContext* LineContext, ELineDepthMode DepthMode) const {
    DirectX::BoundingOrientedBox WorldBox{};
    mObb.Transform(WorldBox, GetComponentToWorld().ToSimpleMath());

    std::array<DirectX::XMFLOAT3, DirectX::BoundingOrientedBox::CORNER_COUNT> Corners{};
    WorldBox.GetCorners(Corners.data());

    const FVector4 LineColor{FVector4{1.0f, 1.0f, 0.0f, 1.0f}};
    const float Thickness{1.0f};
    const auto AddEdge{[LineContext, &Corners, LineColor, Thickness, DepthMode](std::size_t Start, std::size_t End) {
        LineContext->AddLine(FVector3{Corners[Start]}, FVector3{Corners[End]}, LineColor, Thickness, DepthMode);
    }};

    AddEdge(0, 1);
    AddEdge(1, 2);
    AddEdge(2, 3);
    AddEdge(3, 0);
    AddEdge(4, 5);
    AddEdge(5, 6);
    AddEdge(6, 7);
    AddEdge(7, 4);
    AddEdge(0, 4);
    AddEdge(1, 5);
    AddEdge(2, 6);
    AddEdge(3, 7);
}
