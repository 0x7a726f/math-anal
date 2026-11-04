#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/champion_policy.hpp"

using namespace kmp;

int main() {
    ChampionPolicy champ;
    std::cout << "Testing trellis on d=6 across n..." << std::endl;
    for (int n : {64, 128, 256, 512}) {
        auto boxes = generate_trellis_weave(n, 6, 42);
        StructuralCounters c;
        auto t0 = std::chrono::high_resolution_clock::now();
        Int128 vol = KMPSolver::solve(boxes, c, champ);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "trellis d=6 n=" << n << " | ms=" << std::setw(7) << ms << " calls=" << c.recursive_calls << " faces=" << c.total_d2_faces_processed << " vol=" << int128_to_string(vol) << std::endl;
        if (ms > 30000.0) break;
    }
    return 0;
}
