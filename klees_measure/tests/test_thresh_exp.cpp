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
    champ.thresh_d5 = 32;
    champ.thresh_d6 = 22;

    std::cout << "Testing tuned thresholds: thresh_d5=32, thresh_d6=22..." << std::endl;
    for (int n : {256, 512}) {
        auto b5 = generate_trellis_weave(n, 5, 42);
        StructuralCounters c5;
        auto t0 = std::chrono::high_resolution_clock::now();
        Int128 v5 = KMPSolver::solve(b5, c5, champ);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms5 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "d=5 n=" << n << " | ms=" << ms5 << " calls=" << c5.recursive_calls << " faces=" << c5.total_d2_faces_processed << std::endl;
    }

    for (int n : {128, 256}) {
        auto b6 = generate_trellis_weave(n, 6, 42);
        StructuralCounters c6;
        auto t0 = std::chrono::high_resolution_clock::now();
        Int128 v6 = KMPSolver::solve(b6, c6, champ);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms6 = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "d=6 n=" << n << " | ms=" << ms6 << " calls=" << c6.recursive_calls << " faces=" << c6.total_d2_faces_processed << std::endl;
    }
    return 0;
}
