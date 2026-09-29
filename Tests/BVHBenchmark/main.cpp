#include "generated.h"
using Clock = std::chrono::steady_clock;
using namespace DirectX;
volatile double Sink = 0;
int Trial = 0;
struct Result { bool Hit; float Distance; };
struct Data { std::string Name; UMesh Mesh; std::vector<FRay> Rays[2]; std::vector<Result> Expected[2]; };
Uint32 ReadU32(std::ifstream& f) { Uint32 v; f.read(reinterpret_cast<char*>(&v), 4); return v; }
UMesh Load(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f || ReadU32(f) != 0x4d45534d) throw std::runtime_error(path);
    const auto version = ReadU32(f);
    if (version != 3 && version != 4) throw std::runtime_error("version");
    UMesh m;
    m.Positions.resize(ReadU32(f));
    f.read(reinterpret_cast<char*>(m.Positions.data()), m.Positions.size() * sizeof(Position));
    f.seekg(ReadU32(f) * 12LL, std::ios::cur);
    f.seekg(ReadU32(f) * 8LL, std::ios::cur);
    m.Indices.resize(ReadU32(f));
    f.read(reinterpret_cast<char*>(m.Indices.data()), m.Indices.size() * sizeof(Uint32));
    if (!f) throw std::runtime_error("truncated mesh");
    return m;
}
void GenerateRays(Data& d) {
    BoundingBox box;
    BoundingBox::CreateFromPoints(box, d.Mesh.Positions.size(), reinterpret_cast<const XMFLOAT3*>(d.Mesh.Positions.data()), sizeof(Position));
    auto center = XMLoadFloat3(&box.Center);
    float radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&box.Extents)));
    std::mt19937 rng(7361);
    std::uniform_real_distribution<float> u(0, 1);
    for (int axis = 0; axis < 3; ++axis) for (int sign : {-1, 1}) {
        XMFLOAT3 eye{}, right{}, up{};
        (&eye.x)[axis] = sign * radius * 3;
        (&right.x)[(axis+1)%3] = radius;
        (&up.x)[(axis+2)%3] = radius;
        auto origin = XMVectorAdd(center, XMLoadFloat3(&eye));
        for(int y=0;y<32;++y) for(int x=0;x<32;++x) {
            auto target = XMVectorAdd(center, XMVectorAdd(XMVectorScale(XMLoadFloat3(&right), (x+0.5f)/16-1), XMVectorScale(XMLoadFloat3(&up), (y+0.5f)/16-1)));
            auto direction = XMVector3Normalize(XMVectorSubtract(target,origin));
            float t;
            if(box.Intersects(origin,direction,t)) d.Rays[0].emplace_back(origin,direction);
        }
    }
    for(int i=0;i<4096;++i) {
        const auto tri = rng() % (d.Mesh.Indices.size()/3);
        const auto a = d.Mesh.Positions[d.Mesh.Indices[tri*3]].ToSimpleMath();
        const auto b = d.Mesh.Positions[d.Mesh.Indices[tri*3+1]].ToSimpleMath();
        const auto c = d.Mesh.Positions[d.Mesh.Indices[tri*3+2]].ToSimpleMath();
        float s=std::sqrt(u(rng)), t=u(rng);
        auto target=XMVectorAdd(XMVectorScale(a,1-s),XMVectorAdd(XMVectorScale(b,s*(1-t)),XMVectorScale(c,s*t)));
        float z=2*u(rng)-1, phi=XM_2PI*u(rng), r=std::sqrt(1-z*z);
        auto origin=XMVectorAdd(center,XMVectorSet(r*std::cos(phi)*radius*3,r*std::sin(phi)*radius*3,z*radius*3,0));
        d.Rays[1].emplace_back(origin,XMVector3Normalize(XMVectorSubtract(target,origin)));
    }
}
Result Brute(const UMesh& m, const FRay& ray) {
    Result r{false,std::numeric_limits<float>::max()};
    for(size_t i=0;i<m.Indices.size();i+=3) {
        float t;
        if(TriangleTests::Intersects(ray.position,ray.direction,m.Positions[m.Indices[i]].ToSimpleMath(),m.Positions[m.Indices[i+1]].ToSimpleMath(),m.Positions[m.Indices[i+2]].ToSimpleMath(),t) && t<r.Distance) r={true,t};
    }
    return r;
}
bool Equal(Result a, Result b) { return a.Hit==b.Hit && (!a.Hit || std::abs(a.Distance-b.Distance)<=1e-4f*(1+std::abs(a.Distance))); }
template<class B> void Reference(Data& d) {
    B b; B::TraversalCostOverInternalCost=1.2f;
    if(!b.BuildStructure(d.Mesh)) throw std::runtime_error("build");
    for(int k=0;k<2;++k) {
        for(auto& ray:d.Rays[k]) { Result r{false,0}; r.Hit=b.Raycast(d.Mesh,ray,r.Distance); d.Expected[k].push_back(r); }
        for(size_t i=0;i<d.Rays[k].size();i+=std::max<size_t>(1,d.Rays[k].size()/128)) if(!Equal(d.Expected[k][i],Brute(d.Mesh,d.Rays[k][i]))) throw std::runtime_error("brute mismatch");
    }
}
template<class Node> void Count(const Node* n, size_t& nodes, size_t& leaves, size_t& depth, size_t level=0) {
    ++nodes; depth=std::max(depth,level);
    if(n->mLeft) { Count(n->mLeft.get(),nodes,leaves,depth,level+1); Count(n->mRight.get(),nodes,leaves,depth,level+1); }
    else ++leaves;
}
template<class B> void Measure(Data& d, float ratio, FILE* output) {
    B::TraversalCostOverInternalCost=ratio;
    B b;
    auto start=Clock::now();
    if(!b.BuildStructure(d.Mesh)) throw std::runtime_error("build");
    double build=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    size_t nodes=0,leaves=0,depth=0;
    Count(b.root.get(),nodes,leaves,depth);
    for(int k=0;k<2;++k) {
        size_t hits=0;
        for(size_t i=0;i<d.Rays[k].size();++i) {
            Result r{false,0}; r.Hit=b.Raycast(d.Mesh,d.Rays[k][i],r.Distance);
            if(!Equal(r,d.Expected[k][i])) throw std::runtime_error("parameter mismatch");
            hits+=r.Hit;
        }
        double samples[5];
        for(int rep=0;rep<5;++rep) {
            double sum=0;
            start=Clock::now();
            for(auto& ray:d.Rays[k]) { float t=0; if(b.Raycast(d.Mesh,ray,t)) sum+=1+t; }
            samples[rep]=std::chrono::duration<double,std::nano>(Clock::now()-start).count()/d.Rays[k].size();
            Sink=sum;
        }
        std::sort(std::begin(samples),std::end(samples));
        std::fprintf(output,"%s,%zu,%u,%.3f,%s,%zu,%zu,%.6f,%.6f,%.6f,%.6f,%zu,%zu,%zu,%.3f,%d\n",d.Name.c_str(),d.Mesh.Indices.size()/3,B::Slice,ratio,k==0?"camera":"surface",d.Rays[k].size(),hits,build,samples[2],samples[0],samples[4],nodes,leaves,depth,static_cast<double>(d.Mesh.Indices.size()/3)/leaves,Trial);
        std::fflush(output);
    }
}
int main(int argc, char**) {
    try {
        const char* paths[]={"Content/Prisoner_Capoeira.bin","Content/Data/apple_mid.bin","Content/Data/bitten_apple_mid.bin","Content/Models/Monkey/Monkey.bin","Content/System/Mesh/Torus.bin","Content/System/Mesh/Sphere.bin","Content/System/Mesh/SkyDome.bin","Content/Models/Cube/Cube.bin"};
        std::vector<Data> data;
        for(auto path:paths) { Data d; d.Name=path; d.Mesh=Load(path); GenerateRays(d); Reference<BVH8>(d); data.push_back(std::move(d)); }
        struct Job { int Mesh, Slice; float Ratio; int Trial; };
        std::vector<Job> jobs;
        if(argc>1) {
            for(int trial=0;trial<3;++trial) for(int i=0;i<data.size();++i) for(int s:{8,16,32,64}) for(float r:{1.2f,1.5f,2.f,2.5f,3.f,4.f}) jobs.push_back({i,s,r,trial});
        } else {
            for(int i=0;i<data.size();++i) for(int s:{4,8,16,32,64}) for(float r:{0.25f,0.5f,1.f,1.2f,2.f,4.f,8.f}) jobs.push_back({i,s,r,0});
        }
        std::mt19937 rng(9182); std::shuffle(jobs.begin(),jobs.end(),rng);
        FILE* out=std::fopen(argc>1?"Tests/BVHBenchmark/refined.csv":"Tests/BVHBenchmark/results.csv","w");
        if(!out) throw std::runtime_error("output");
        std::fprintf(out,"mesh,triangles,slice,ratio,rays,rays_count,hits,build_ms,median_ns,min_ns,max_ns,nodes,leaves,depth,tris_per_leaf,trial\n");
        int done=0;
        for(auto j:jobs) {
            Trial=j.Trial;
            auto& d=data[j.Mesh];
            switch(j.Slice) {
                case 4: Measure<BVH4>(d,j.Ratio,out); break;
                case 8: Measure<BVH8>(d,j.Ratio,out); break;
                case 16: Measure<BVH16>(d,j.Ratio,out); break;
                case 32: Measure<BVH32>(d,j.Ratio,out); break;
                case 64: Measure<BVH64>(d,j.Ratio,out); break;
            }
            if(++done%20==0) { std::printf("%d/%zu configurations complete\n",done,jobs.size()); std::fflush(stdout); }
        }
        std::fclose(out);
        std::puts("PASS: all configurations match reference; reference sampled against brute force");
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
