#include <iostream>
#include <chrono>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/split_policy.hpp"

using namespace kmp;

int main() {
    SplitPolicy champion_policy = get_split_policy();
    std::cout << "Testing timings across dimensions..." << std::endl;
    for (int d = 3; d <= 6; ++d) {
        for (int n : {10, 20, 35, 64, 128}) {
            auto boxes = generate_trellis_weave(n, d, 42);
            // Sweep time
            double sweep_ms = -1;
            if (n <= (d >= 5 ? 35 : (d == 4 ? 64 : 128))) {
                auto t0 = std::chrono::high_resolution_clock::now();
                Int128 vol_sw = exact_kmp_sweep(boxes, d);
                auto t1 = std::chrono::high_resolution_clock::now();
                sweep_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            }

            StructuralCounters counters;
            auto t2 = std::chrono::high_resolution_clock::now();
            Int128 vol_champ = KMPSolver::solve(boxes, counters, champion_policy);
            auto t3 = std::chrono::high_resolution_clock::now();
            double champ_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

            std::cout << "d=" << d << " n=" << n
                      << " | Champ: " << champ_ms << " ms (calls=" << counters.recursive_calls << ", faces=" << counters.total_d2_faces_processed << ")"
                      << " | Sweep: " << sweep_ms << " ms"
                      << std::endl;
        }
    }
    return 0;
}
