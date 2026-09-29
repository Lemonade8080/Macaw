#include "traversal-common.h"
#include "Core/Base/FTransform.h"

struct TestMesh : UMesh {
    After Tree;
    bool Raycast(const FRay& ray, float& distance, float limit) const { return Tree.Raycast(*this, ray, distance, limit); }
};
struct TestComponent {
    TestMesh* Mesh;
    FTransform Transform;
    const TestMesh* ResolveMesh() const { return Mesh; }
    FTransform GetComponentTransform() const { return Transform; }
    bool RaycastMesh(const FRay& Ray, float& OutDistance, float MaxDistance) const;
};
#include "traversal-component.h"

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void CheckLimits(const After& tree, const UMesh& mesh, const FRay& ray, Result expected) {
    for (float limit : {0.0f, 0.5f, 5.0f, 1000.0f, expected.Hit ? expected.Distance : 10.0f}) {
        float distance = -123;
        bool hit = tree.Raycast(mesh, ray, distance, limit);
        Check(hit == (expected.Hit && expected.Distance <= limit), "limited ray hit mismatch");
        Check(!hit || Equal({hit,distance},expected), "limited ray distance mismatch");
        Check(hit || distance == -123, "miss changed output");
    }
}
void BoundaryTests() {
    TestMesh mesh;
    mesh.Positions = {Position{-1,-1,0}, Position{1,-1,0}, Position{0,1,0}};
    mesh.Indices = {0,1,2};
    Check(mesh.Tree.BuildStructure(mesh), "single build");
    std::vector<FRay> rays;
    for (float x : {-1.0f, 0.0f, 1.0f, 1.001f}) for (float y : {-1.0f, 0.0f, 1.0f}) {
        rays.emplace_back(XMVectorSet(x,y,5,0), XMVectorSet(0,0,-1,0));
        rays.emplace_back(XMVectorSet(x,y,0,0), XMVectorSet(-0.0f,0,1,0));
    }
    rays.emplace_back(XMVectorSet(0,0,5,0), XMVectorSet(0,0,1,0));
    rays.emplace_back(XMVectorSet(0,0,5,0), XMVector3Normalize(XMVectorSet(1e-10f,0,-1,0)));
    rays.emplace_back(XMVectorSet(0,0,0,0), XMVectorSet(1,0,0,0));
    for (const auto& ray : rays) CheckLimits(mesh.Tree,mesh,ray,Brute(mesh,ray));
    float distance = -1;
    Check(!mesh.Tree.Raycast(mesh,rays[0],distance,-1), "negative limit");
    Check(!mesh.Tree.Raycast(mesh,rays[0],distance,std::numeric_limits<float>::quiet_NaN()), "NaN limit");
    for (auto scale : {FVector3{2,3,4}, FVector3{-2,3,0.5f}, FVector3{0.25f,-4,-2}}) {
        TestComponent component{&mesh, FTransform{FVector3{3,4,5}, FRotator{20,35,10}, scale}};
        const auto rotation = component.Transform.GetRotationQuaternion().ToSimpleMath();
        const auto position = component.Transform.GetPosition().ToSimpleMath();
        const auto origin = XMVectorAdd(position,XMVector3Rotate(XMVectorSet(0,0,5*scale.mZ,0),rotation));
        const auto direction = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0,0,-scale.mZ,0),rotation));
        float expected = 5*std::abs(scale.mZ);
        FRay ray{origin,direction};
        Check(component.RaycastMesh(ray,distance,expected+0.01f) && std::abs(distance-expected)<0.001f, "scaled component hit");
        Check(!component.RaycastMesh(ray,distance,expected-0.01f), "scaled component limit");
    }
}
void OverflowTest() {
    UMesh mesh;
    mesh.Positions = {Position{-1,-1,0}, Position{1,-1,0}, Position{0,1,0}, Position{5,5,0}, Position{6,5,0}, Position{5,6,0}};
    mesh.Indices = {0,1,2,3,4,5};
    After tree;
    tree.Nodes.resize(141); tree.mIndexGroups = {0,1};
    for (auto& n : tree.Nodes) { n.BoundingBox.Center = {0,0,0}; n.BoundingBox.Extents = {10,10,10}; n.mIndexStart = 1; n.mIndexCount = 1; }
    for (Uint32 i=0; i<70; ++i) { tree.Nodes[i].mIndexCount=0; tree.Nodes[i].mLeft=i+1; tree.Nodes[i].mRight=71+i; }
    tree.Nodes[71].mIndexStart=0;
    float distance = 0;
    Check(tree.Raycast(mesh,FRay{XMVectorSet(0,0,5,0),XMVectorSet(0,0,-1,0)},distance) && distance==5, "stack overflow fallback");
}
template<class B> void Measure(Data& data, const char* name, int trial, FILE* output) {
    B tree; Check(tree.BuildStructure(data.Mesh), "build");
    size_t hits=0;
    for (size_t i=0;i<data.Rays[0].size();++i) {
        Result result{false,0}; result.Hit=tree.Raycast(data.Mesh,data.Rays[0][i],result.Distance);
        Check(Equal(result,data.Expected[0][i]), "baseline mismatch"); hits+=result.Hit;
    }
    double times[5];
    auto allocations = Stat::GetStats().mMemory.mTotalAllocationCount;
    for (int repeat=0;repeat<5;++repeat) {
        double sum=0; auto start=Clock::now();
        for (const auto& ray:data.Rays[0]) { float distance=0; if(tree.Raycast(data.Mesh,ray,distance)) sum+=1+distance; }
        times[repeat]=std::chrono::duration<double,std::nano>(Clock::now()-start).count()/data.Rays[0].size(); Sink=sum;
    }
    allocations=Stat::GetStats().mMemory.mTotalAllocationCount-allocations;
    std::sort(std::begin(times),std::end(times));
    std::fprintf(output,"%s,%d,%.6f,%zu,%zu,%zu,%zu\n",name,trial,times[2],allocations,data.Rays[0].size(),hits,sizeof(typename B::FNode));
}
int main() {
    try {
        BoundaryTests(); OverflowTest();
        Data data; data.Mesh=Load("Content/Prisoner_Capoeira.bin"); GenerateRays(data);
        Before before; After after; Check(before.BuildStructure(data.Mesh) && after.BuildStructure(data.Mesh),"reference build");
        for (const auto& ray:data.Rays[0]) { Result result{false,0}; result.Hit=before.Raycast(data.Mesh,ray,result.Distance); data.Expected[0].push_back(result); }
        for (size_t i=0;i<data.Rays[0].size();++i) {
            CheckLimits(after,data.Mesh,data.Rays[0][i],data.Expected[0][i]);
            if (i%41==0) Check(Equal(data.Expected[0][i],Brute(data.Mesh,data.Rays[0][i])),"brute mismatch");
        }
        FILE* output=std::fopen("Tests/BVHBenchmark/traversal-results.csv","w"); Check(output!=nullptr,"output");
        std::fprintf(output,"variant,trial,ray_ns,allocations_5_batches,rays,hits,node_bytes\n");
        for (int trial=0;trial<5;++trial) {
            if (trial%2) { Measure<After>(data,"after",trial,output); Measure<Before>(data,"before",trial,output); }
            else { Measure<Before>(data,"before",trial,output); Measure<After>(data,"after",trial,output); }
        }
        std::fclose(output);
        std::puts("PASS: camera rays, distance caps, brute samples, boundary rays, transformed components and stack overflow fallback");
    } catch (const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
