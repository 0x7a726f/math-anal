#include <iostream>
#include <chrono>
#include <iomanip>
#include <cassert>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/champion_policy.hpp"
#include "../solver/baseline_policy.hpp"

using namespace kmp;

int main() {
    ChampionPolicy champ;
    BaselinePolicy base;

    std::cout << "Testing Champion vs Baseline across d={3,4,5,6} and n={64..1024}..." << std::endl;

    for (int d : {3, 4, 5, 6}) {
        for (int n : {64, 128, 256, 512, 1024}) {
            auto boxes = generate_trellis_weave(n, d, 42);

            StructuralCounters c_base;
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 v_base = KMPSolver::solve(boxes, c_base, base);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms_base = std::chrono::duration<double, std::milli>(t1 - t0).count();

            StructuralCounters c_champ;
            auto t2 = std::chrono::high_resolution_clock::now();
            Int128 v_champ = KMPSolver::solve(boxes, c_champ, champ);
            auto t3 = std::chrono::high_resolution_clock::now();
            double ms_champ = std::chrono::duration<double, std::milli>(t3 - t2).count();

            bool exact_match = (v_base == v_champ);
            double speedup_time = ms_base / std::max(0.001, ms_champ);
            double speedup_faces = (double)c_base.total_d2_faces_processed / std::max(1ULL, c_champ.total_d2_faces_processed);

            std::cout << "[d=" << d << ", n=" << std::setw(4) << n << "] "
                      << "Base: " << std::setw(7) << ms_base << " ms (" << std::setw(6) << c_base.total_d2_faces_processed << " faces, " << std::setw(5) << c_base.recursive_calls << " calls) | "
                      << "Champ: " << std::setw(7) << ms_champ << " ms (" << std::setw(6) << c_champ.total_d2_faces_processed << " faces, " << std::setw(5) << c_champ.recursive_calls << " calls) | "
                      << "Speedup: " << std::setw(5) << std::fixed << std::setprecision(2) << speedup_time << "x (faces: " << speedup_faces << "x) | "
                      << "Match: " << (exact_match ? "YES" : "FAIL")
                      << std::endl;
        }
    }
    return 0;
}
