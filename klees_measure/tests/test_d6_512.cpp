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
    std::cout << "Testing Champion on d=6, n=512 trellis..." << std::endl;
    auto boxes = generate_trellis_weave(512, 6, 42);
    StructuralCounters c;
    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 vol = KMPSolver::solve(boxes, c, champ);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "d=6 n=512 trellis | ms=" << ms << " calls=" << c.recursive_calls << " faces=" << c.total_d2_faces_processed << " vol=" << int128_to_string(vol) << std::endl;
    return 0;
}
