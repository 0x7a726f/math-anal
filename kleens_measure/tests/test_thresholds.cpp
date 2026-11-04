#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/split_policy.hpp"

using namespace kmp;

int main() {
    SplitPolicy policy = get_split_policy();

    for (int leaf_k : {8, 16, 24, 32, 48, 64}) {
        std::cout << "\n=== Testing leaf threshold k = " << leaf_k << " ===" << std::endl;
        policy.leaf_box_threshold = leaf_k;

        for (int d : {3, 4, 5, 6}) {
            for (int n : {64, 128, 256, 512, 1024}) {
                auto boxes = generate_trellis_weave(n, d, 42);
                StructuralCounters counters;
                auto t0 = std::chrono::high_resolution_clock::now();
                Int128 vol = KMPSolver::solve(boxes, counters, policy);
                auto t1 = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

                std::cout << "k=" << leaf_k << " d=" << d << " n=" << std::setw(4) << n
                          << " | time: " << std::setw(7) << ms << " ms"
                          << " | calls: " << std::setw(6) << counters.recursive_calls
                          << " | faces: " << std::setw(8) << counters.total_d2_faces_processed
                          << std::endl;
                if (ms > 2000.0) {
                    std::cout << "Breaking early due to high time\n";
                    break;
                }
            }
        }
    }
    return 0;
}
