#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/clean_cell_trace.hpp"

using namespace kmp;

int main() {
    std::cout << "================================================================" << std::endl;
    std::cout << "Benchmark: Pure Combinatorial Sweep vs C++ Matrix-Trace Factorization" << std::endl;
    std::cout << "Instance: 3D Orthogonal Trellis Weaves" << std::endl;
    std::cout << "================================================================" << std::endl;

    for (int n : {50, 100, 200, 400}) {
        auto boxes = generate_trellis_weave(n, 3, 42);
        Box root_cell;
        root_cell.low = {0, 0, 0};
        root_cell.high = {1000000, 1000000, 1000000};

        // Sweep time
        auto t0 = std::chrono::high_resolution_clock::now();
        Int128 vol_sweep = exact_kmp_sweep(boxes, 3);
        auto t1 = std::chrono::high_resolution_clock::now();
        double sweep_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Trace time
        auto t2 = std::chrono::high_resolution_clock::now();
        Int128 vol_trace = evaluate_clean_cell_d3_trace(root_cell, boxes);
        auto t3 = std::chrono::high_resolution_clock::now();
        double trace_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

        if (vol_sweep != vol_trace) {
            std::cerr << "Mismatch on n=" << n << std::endl;
            return 1;
        }

        double speedup = sweep_ms / trace_ms;
        std::cout << "n = " << std::setw(4) << n 
                  << " | Sweep: " << std::setw(8) << std::fixed << std::setprecision(2) << sweep_ms << " ms"
                  << " | Matrix Trace: " << std::setw(8) << std::fixed << std::setprecision(2) << trace_ms << " ms"
                  << " | Speedup: " << std::setw(6) << std::fixed << std::setprecision(1) << speedup << "x"
                  << std::endl;
    }

    return 0;
}
