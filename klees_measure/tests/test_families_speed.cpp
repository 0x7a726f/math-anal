#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/champion_policy.hpp"

using namespace kmp;

inline std::vector<Box> generate_random_uniform_boxes(int n, int d, uint64_t seed = 42) {
    FastRng rng(seed);
    const int64_t M = 1000000;
    std::vector<Box> boxes;
    boxes.reserve(n);
    for (int k = 0; k < n; ++k) {
        Box b;
        b.low.resize(d);
        b.high.resize(d);
        for (int i = 0; i < d; ++i) {
            int64_t u = rng.next_range(0, M - 50000);
            int64_t span = rng.next_range(50000, 300000);
            b.low[i] = u;
            b.high[i] = std::min(M, u + span);
        }
        boxes.push_back(b);
    }
    return boxes;
}

int main() {
    ChampionPolicy champ;
    std::cout << "Testing families on d=5, n=256 and n=512..." << std::endl;
    for (const std::string& fam : {"random", "hyperclique", "trellis"}) {
        for (int n : {256, 512}) {
            std::vector<Box> boxes;
            if (fam == "trellis") boxes = generate_trellis_weave(n, 5, 42);
            else if (fam == "hyperclique") boxes = generate_hyperclique_gadget(n, 5, 999);
            else boxes = generate_random_uniform_boxes(n, 5, 777);

            StructuralCounters c;
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 vol = KMPSolver::solve(boxes, c, champ);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "fam=" << std::setw(11) << fam << " n=" << n << " | ms=" << std::setw(7) << ms << " calls=" << c.recursive_calls << " faces=" << c.total_d2_faces_processed << std::endl;
        }
    }
    return 0;
}
