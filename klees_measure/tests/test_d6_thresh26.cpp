#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/champion_policy.hpp"

using namespace kmp;

int main() {
    ChampionPolicy champ;
    champ.thresh_d6 = 26;
    std::cout << "Testing d=6 n=256 with thresh_d6=26 and zero-alloc sweep..." << std::endl;
    auto boxes = generate_trellis_weave(256, 6, 42);
    StructuralCounters c;
    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 vol = KMPSolver::solve(boxes, c, champ);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "d=6 n=256 | ms=" << ms << " calls=" << c.recursive_calls << " faces=" << c.total_d2_faces_processed << std::endl;
    return 0;
}
