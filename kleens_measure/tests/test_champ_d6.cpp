#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"
#include "test_threshold_tuning.cpp"

using namespace kmp;

int main() {
    TunedChampionPolicy policy;
    std::cout << "Testing Champion on d=6 across all n..." << std::endl;
    for (int n : {64, 128, 256, 512, 1024}) {
        auto boxes = generate_trellis_weave(n, 6, 42);
        StructuralCounters c;
        auto t0 = std::chrono::high_resolution_clock::now();
        Int128 vol = KMPSolver::solve(boxes, c, policy);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "d=6 n=" << std::setw(4) << n << " | ms=" << std::setw(7) << ms << " calls=" << std::setw(6) << c.recursive_calls << " faces=" << std::setw(8) << c.total_d2_faces_processed << " leaves=" << c.trellis_leaves << std::endl;
    }
    return 0;
}
