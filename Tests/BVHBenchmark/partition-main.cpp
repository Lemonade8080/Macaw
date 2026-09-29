#include "partition-common.h"

template<class B> void Validate(const B& b, size_t triangles) {
    if (b.mIndexGroups.size() != triangles || b.Nodes.empty()) throw std::runtime_error("invalid tree size");
    std::vector<unsigned char> ids(triangles), slots(triangles), visited(b.Nodes.size());
    std::vector<Uint32> pending{0};
    while (!pending.empty()) {
        Uint32 index = pending.back(); pending.pop_back();
        if (index >= b.Nodes.size() || visited[index]++) throw std::runtime_error("invalid node links");
        const auto& n = b.Nodes[index];
        if (!n.mIndexCount) { pending.push_back(n.mLeft); pending.push_back(n.mRight); continue; }
        if (static_cast<size_t>(n.mIndexStart) + n.mIndexCount > triangles) throw std::runtime_error("invalid leaf range");
        for (Uint32 j = 0; j < n.mIndexCount; ++j) {
            Uint32 slot = n.mIndexStart + j, id = b.mIndexGroups[slot];
            if (slots[slot]++ || id >= triangles || ids[id]++) throw std::runtime_error("duplicate triangle or range");
        }
    }
    if (std::count(ids.begin(), ids.end(), 1) != triangles || std::count(visited.begin(), visited.end(), 1) != b.Nodes.size()) throw std::runtime_error("missing triangle or node");
}

template<class B> void EdgeCases() {
    B b; UMesh m;
    if (b.BuildStructure(m)) throw std::runtime_error("empty accepted");
    m.Positions = {Position{0,0,0}, Position{1,0,0}, Position{0,1,0}};
    m.Indices = {0,1,2};
    if (!b.BuildStructure(m)) throw std::runtime_error("single triangle");
    Validate(b, 1);
    for (int i = 0; i < 16; ++i) m.Indices.insert(m.Indices.end(), {0,1,2});
    if (!b.BuildStructure(m)) throw std::runtime_error("same centroid");
    Validate(b, 17);
    m.Positions = {Position{0,0,0}, Position{0,0,0}, Position{0,0,0}};
    if (!b.BuildStructure(m)) throw std::runtime_error("degenerate bounds");
    Validate(b, 17);
    m.Indices.push_back(0);
    if (b.BuildStructure(m) || !b.Nodes.empty()) throw std::runtime_error("invalid count accepted");
    m.Indices = {0,1,3};
    if (b.BuildStructure(m)) throw std::runtime_error("invalid index accepted");
}

template<class B> void MeasureOne(Data& d, const char* name, float ratio, int trial, FILE* output) {
    B::TraversalCostOverInternalCost = ratio;
    B b;
    auto start = Clock::now();
    if (!b.BuildStructure(d.Mesh)) throw std::runtime_error("build");
    const double build = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    Validate(b, d.Mesh.Indices.size() / 3);
    size_t hits = 0, leaves = 0;
    for (const auto& node : b.Nodes) leaves += node.mIndexCount != 0;
    for (size_t i = 0; i < d.Rays[0].size(); ++i) {
        Result r{false, 0}; r.Hit = b.Raycast(d.Mesh, d.Rays[0][i], r.Distance);
        if (!Equal(r, d.Expected[0][i])) throw std::runtime_error("ray mismatch");
        hits += r.Hit;
    }
    double samples[5];
    for (int repeat = 0; repeat < 5; ++repeat) {
        double sum = 0;
        start = Clock::now();
        for (const auto& ray : d.Rays[0]) { float t = 0; if (b.Raycast(d.Mesh, ray, t)) sum += 1 + t; }
        samples[repeat] = std::chrono::duration<double, std::nano>(Clock::now() - start).count() / d.Rays[0].size();
        Sink = sum;
    }
    std::sort(std::begin(samples), std::end(samples));
    std::fprintf(output, "%s,%u,%.1f,%d,%.6f,%.6f,%zu,%zu,%zu,%zu\n", name, B::Slice, ratio, trial, build, samples[2], b.Nodes.size(), leaves, d.Rays[0].size(), hits);
    std::fflush(output);
}

int main() {
    try {
        EdgeCases<Partition32>();
        Data d; d.Name = "Content/Prisoner_Capoeira.bin"; d.Mesh = Load(d.Name.c_str()); GenerateRays(d);
        Baseline32 reference;
        if (!reference.BuildStructure(d.Mesh)) throw std::runtime_error("reference build");
        Validate(reference, d.Mesh.Indices.size() / 3);
        for (const auto& ray : d.Rays[0]) { Result r{false,0}; r.Hit = reference.Raycast(d.Mesh,ray,r.Distance); d.Expected[0].push_back(r); }
        size_t bruteChecks = 0;
        for (size_t i = 0; i < d.Rays[0].size(); i += std::max<size_t>(1, d.Rays[0].size() / 64)) {
            if (!Equal(d.Expected[0][i], Brute(d.Mesh, d.Rays[0][i]))) throw std::runtime_error("brute mismatch");
            ++bruteChecks;
        }
        struct Job { int Variant; float Ratio; int Trial; };
        std::vector<Job> jobs;
        for (int trial = 0; trial < 5; ++trial) {
            jobs.push_back({0, 2.5f, trial}); jobs.push_back({16, 2.5f, trial}); jobs.push_back({32, 2.5f, trial}); jobs.push_back({64, 2.5f, trial});
            jobs.push_back({32, 2.0f, trial}); jobs.push_back({32, 3.0f, trial});
        }
        std::mt19937 rng(9182); std::shuffle(jobs.begin(), jobs.end(), rng);
        FILE* out = std::fopen("Tests/BVHBenchmark/partition-results.csv", "w");
        if (!out) throw std::runtime_error("output");
        std::fprintf(out, "variant,bins,ratio,trial,build_ms,ray_ns,nodes,leaves,rays,hits\n");
        for (auto j : jobs) {
            switch (j.Variant) {
                case 0: MeasureOne<Baseline32>(d, "baseline", j.Ratio, j.Trial, out); break;
                case 16: MeasureOne<Partition16>(d, "partition", j.Ratio, j.Trial, out); break;
                case 32: MeasureOne<Partition32>(d, "partition", j.Ratio, j.Trial, out); break;
                case 64: MeasureOne<Partition64>(d, "partition", j.Ratio, j.Trial, out); break;
            }
        }
        std::fclose(out);
        std::printf("PASS: %zu triangles, %zu camera rays, %zu brute checks, 30 builds; all ray results, leaf coverage, links and edge cases verified\n", d.Mesh.Indices.size()/3, d.Rays[0].size(), bruteChecks);
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
