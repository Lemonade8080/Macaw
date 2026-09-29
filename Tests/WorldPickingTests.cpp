#include "pch.h"
#include "World/Subsystem/UPickingSubsystem.h"
#include "World/Component/UBillboardComponent.h"
#include "World/Component/UMeshComponent.h"
#include <iostream>
#include <random>
#include <stdexcept>

void Check(bool Value, const char* Message) { if (!Value) throw std::runtime_error(Message); }
struct FObjects {
    std::vector<std::unique_ptr<UPrimitiveComponent>> Objects;
    template<class T = UPrimitiveComponent> T* Add() {
        auto Object = std::make_unique<T>();
        UObjectSystem::Register(Object.get());
        T* Result = Object.get(); Objects.push_back(std::move(Object)); return Result;
    }
    ~FObjects() { for (auto& Object : Objects) UObjectSystem::Unregister(Object.get(), Object->GetHandle()); }
};
struct FTestMesh : UMeshComponent {
    const UMesh* Mesh = nullptr;
    mutable int Queries = 0;
    const UMesh* ResolveMesh() const override { ++Queries; return Mesh; }
};
FRay Ray(FVector3 Origin, FVector3 Target) { return {Origin.ToSimpleMath(), DirectX::XMVector3Normalize((Target-Origin).ToSimpleMath())}; }
bool Linear(const TArray<TObjectRef<UPrimitiveComponent>>& Components, const FRay& Ray, UPrimitiveComponent*& Out, float& Distance, const FMatrix* Camera = nullptr) {
    Out = nullptr; Distance = std::numeric_limits<float>::max();
    for (const auto& Ref : Components) {
        auto* Component = Ref.Get();
        if (!Component || !Component->IsActive() || !Component->IsVisible()) continue;
        float Hit = 0;
        if (Component->GetTypeInfo()->IsA(UBillboardComponent::StaticTypeInfo())) {
            if (!Camera) continue;
            std::array<FVector3,4> Corners;
            if (!static_cast<UBillboardComponent*>(Component)->GetWorldCorners(*Camera,Corners)) continue;
            float A = 0, B = 0;
            bool HA = DirectX::TriangleTests::Intersects(Ray.position,Ray.direction,Corners[0].ToSimpleMath(),Corners[1].ToSimpleMath(),Corners[2].ToSimpleMath(),A);
            bool HB = DirectX::TriangleTests::Intersects(Ray.position,Ray.direction,Corners[1].ToSimpleMath(),Corners[3].ToSimpleMath(),Corners[2].ToSimpleMath(),B);
            if (!HA && !HB) continue;
            Hit = HA && HB ? (std::min)(A,B) : HA ? A : B;
        }
        else {
            DirectX::BoundingOrientedBox Box;
            Component->GetPickingBox().Transform(Box,Component->GetComponentToWorld().ToSimpleMath());
            if (!Box.Intersects(Ray.position,Ray.direction,Hit)) continue;
            Hit = (std::max)(Hit,0.0f);
            if (Component->GetTypeInfo()->IsA(UMeshComponent::StaticTypeInfo()) && !static_cast<UMeshComponent*>(Component)->RaycastMesh(Ray,Hit)) continue;
        }
        if (Hit < Distance) { Out = Component; Distance = Hit; }
    }
    return Out != nullptr;
}
void Compare(const UPickingSubsystem& Picking, const FRay& Ray, const FMatrix* Camera = nullptr) {
    UPrimitiveComponent* A = nullptr; UPrimitiveComponent* B = nullptr; float DA = 0, DB = 0;
    bool HA = Linear(Picking.GetRegisteredComponents(),Ray,A,DA,Camera);
    bool HB = Picking.Raycast(Ray,B,DB,Camera);
    Check(HA == HB,"hit mismatch");
    if (HA) { Check(std::abs(DA-DB) <= 1e-4f*(1+std::abs(DA)),"distance mismatch"); Check(A==B,"component mismatch"); }
    else Check(!B && DB==std::numeric_limits<float>::max(),"empty result");
}
void Boxes() {
    FObjects Objects; UPickingSubsystem Picking;
    Compare(Picking,Ray({0,0,60},{0,0,0}));
    std::mt19937 Random(1947); std::uniform_real_distribution<float> Position(-25,25), Size(0.2f,1.5f), Angle(-170,170);
    for (int i=0;i<257;++i) {
        auto* Component=Objects.Add();
        Component->SetPickingBox({{0,0,0},{Size(Random),Size(Random),Size(Random)},{0,0,0,1}});
        Component->SetRelativeTransform(FTransform{{Position(Random),Position(Random),Position(Random)},FRotator{Angle(Random),Angle(Random),Angle(Random)},{i%2 ? -1.3f:0.7f,1.1f,2.0f}});
        Picking.RegisterComponent(Component);
    }
    Check(Picking.RebuildAccelerationStructure(),"build boxes");
    for (const auto& Object:Objects.Objects) Compare(Picking,Ray({0,0,60},Object->GetComponentTransform().GetPosition()));
    for (int i=0;i<300;++i) Compare(Picking,Ray({Position(Random),Position(Random),60},{Position(Random),Position(Random),Position(Random)}));
    auto* First=Objects.Objects[0].get();
    First->SetVisible(false); First->SetActive(false);
    Check(Picking.RebuildAccelerationStructure(),"hidden build");
    First->SetVisible(true); First->SetActive(true);
    Compare(Picking,Ray({0,0,60},First->GetComponentTransform().GetPosition()));
    Picking.UnregisterComponent(First);
    Check(!Picking.ContainsComponent(First),"unregister");
    Check(Picking.RebuildAccelerationStructure(),"explicit rebuild after unregister");
    Compare(Picking,Ray({0,0,60},First->GetComponentTransform().GetPosition()));
    UObjectSystem::Unregister(Objects.Objects[1].get(),Objects.Objects[1]->GetHandle());
    Compare(Picking,Ray({0,0,60},Objects.Objects[1]->GetComponentTransform().GetPosition()));
    First->SetRelativeLocation({100,0,0}); Picking.RegisterComponent(First);
    Check(Picking.RebuildAccelerationStructure(),"explicit rebuild after register");
    Compare(Picking,Ray({100,0,60},{100,0,0}));
    First->SetRelativeLocation({150,0,0}); Check(Picking.RebuildAccelerationStructure(),"explicit rebuild");
    Compare(Picking,Ray({150,0,60},{150,0,0}));
}
void Coincident() {
    FObjects Objects; UPickingSubsystem Picking;
    for (int i=0;i<129;++i) {
        auto* Component=Objects.Add(); Component->SetPickingBox({{0,0,0},{1,1,1},{0,0,0,1}}); Component->SetVisible(false); Picking.RegisterComponent(Component);
    }
    Check(Picking.RebuildAccelerationStructure(),"identical centers");
    for (auto& Object:Objects.Objects) { Object->SetVisible(true); Compare(Picking,Ray({0,0,10},{0,0,0})); Compare(Picking,Ray({0,0,0},{1,0,0})); Object->SetVisible(false); }
    for (auto& Object:Objects.Objects) Object->SetPickingBox({{0,0,0},{0,0,0},{0,0,0,1}});
    Check(Picking.RebuildAccelerationStructure(),"point bounds");
    Objects.Objects[0]->SetVisible(true); Compare(Picking,Ray({0,0,10},{0,0,0}));
}
void Billboards() {
    FObjects Objects; UPickingSubsystem Picking;
    auto* Billboard=Objects.Add<UBillboardComponent>(); Billboard->SetSize({4,2}); Billboard->SetTextureHandle({1,1}); Billboard->SetPipelineHandle({2,1}); Picking.RegisterComponent(Billboard);
    Check(Picking.RebuildAccelerationStructure(),"billboard build");
    for (float Angle:{0.0f,0.7f,1.5f}) {
        FMatrix Camera=FMatrix::CreateRotationY(Angle); std::array<FVector3,4> Corners;
        Check(Billboard->GetWorldCorners(Camera,Corners),"billboard corners");
        FVector3 Normal=(Corners[2]-Corners[0]).Cross(Corners[1]-Corners[0]); Normal.Normalize();
        for (float U:{0.1f,0.5f,0.9f,1.1f}) {
            FVector3 Target=Corners[0]+(Corners[2]-Corners[0])*U+(Corners[1]-Corners[0])*0.4f;
            Compare(Picking,Ray(Target+Normal*10,Target),&Camera);
        }
    }
    Compare(Picking,Ray({0,0,10},{0,0,0}));
}
void Meshes() {
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&Device,nullptr,nullptr)),"WARP device");
    UMesh Mesh; Check(Mesh.Initialize(Device.Get(),{},"Content/System/Mesh/Torus.bin",{},{},false),"load mesh");
    FObjects Objects; UPickingSubsystem Picking;
    for (int i=0;i<9;++i) {
        auto* Component=Objects.Add<FTestMesh>(); Component->Mesh=&Mesh; Component->SetPickingBox(Mesh.GetBoundingBox());
        Component->SetRelativeTransform(FTransform{{static_cast<float>(i%3)*5,static_cast<float>(i/3)*5,0},FRotator{15,30,20},{i%2 ? -1.0f:1.0f,2,0.5f}});
        Picking.RegisterComponent(Component);
    }
    Check(Picking.RebuildAccelerationStructure(),"mesh build");
    for (int y=-4;y<25;++y) for (int x=-4;x<25;++x) Compare(Picking,Ray({x*0.5f,y*0.5f,20},{x*0.5f,y*0.5f,0}));
    UPickingSubsystem Pruning;
    auto* Near=Objects.Add(); Near->SetPickingBox({{0,0,0},{2,2,2},{0,0,0,1}}); Near->SetRelativeLocation({0,0,10}); Pruning.RegisterComponent(Near);
    auto* Far=Objects.Add<FTestMesh>(); Far->Mesh=&Mesh; Far->SetPickingBox(Mesh.GetBoundingBox()); Pruning.RegisterComponent(Far);
    UPrimitiveComponent* Hit=nullptr; float Distance=0;
    Check(Pruning.RebuildAccelerationStructure(),"pruning build");
    Check(Pruning.Raycast(Ray({0,0,20},{0,0,0}),Hit,Distance) && Hit==Near && Far->Queries==0,"nearest hit did not prune far mesh");
}
void ExplicitBuildOnly() {
    FObjects Objects; UPickingSubsystem Picking;
    auto* First=Objects.Add(); First->SetPickingBox({{0,0,0},{1,1,1},{0,0,0,1}}); Picking.RegisterComponent(First);
    UPrimitiveComponent* Hit=nullptr; float Distance=0;
    Check(!Picking.Raycast(Ray({0,0,10},{0,0,0}),Hit,Distance),"first query must not build");
    Check(Picking.RebuildAccelerationStructure(),"explicit initial build");
    Check(Picking.Raycast(Ray({0,0,10},{0,0,0}),Hit,Distance) && Hit==First,"explicit build result");
    auto* Second=Objects.Add(); Second->SetPickingBox({{0,0,0},{1,1,1},{0,0,0,1}}); Second->SetRelativeLocation({20,0,0}); Picking.RegisterComponent(Second);
    Check(!Picking.Raycast(Ray({20,0,10},{20,0,0}),Hit,Distance),"registration must not rebuild");
    Picking.UnregisterComponent(First);
    Check(!Picking.Raycast(Ray({20,0,10},{20,0,0}),Hit,Distance),"unregistration must not rebuild");
    Check(Picking.RebuildAccelerationStructure(),"explicit replacement build");
    Compare(Picking,Ray({20,0,10},{20,0,0}));
    Compare(Picking,Ray({0,0,10},{0,0,0}));
}
int main() {
    try { ExplicitBuildOnly(); Boxes(); Coincident(); Billboards(); Meshes(); std::cout<<"PASS: explicit builds only, world BVH vs linear, coincident/point bounds, lifetimes, visibility, camera billboards, real mesh narrow phase, closest-hit pruning\n"; }
    catch (const std::exception& Error) { std::cerr<<"FAIL: "<<Error.what()<<'\n'; return 1; }
}
