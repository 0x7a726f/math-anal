#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "harness/types.hpp"
#include "harness/generators.hpp"
#include "harness/verifier.hpp"
#include "solver/tensor_clean_cell.hpp"

using namespace kmp;

int main() {
    std::cout << "================================================================" << std::endl;
    std::cout << " Crossover Benchmark: Tensor Contraction vs Sweep in d=5 and d=6" << std::endl;
    std::cout << "================================================================" << std::endl;

    for (int d : {5, 6}) {
        std::cout << "\n--- Dimension d = " << d << " ---" << std::endl;
        std::cout << "  n | Tensor (us) | Sweep (us) | Speedup | Exact Match" << std::endl;
        std::cout << "----+-------------+------------+---------+------------" << std::endl;

        for (int n : {4, 8, 12, 16, 20, 24, 28, 32}) {
            const int ITERS = 100;
            double tensor_tot_us = 0.0;
            double sweep_tot_us = 0.0;
            bool all_match = true;

            for (int it = 0; it < ITERS; ++it) {
                auto boxes = generate_trellis_weave(n, d, 1000 + it);
                Box cell;
                cell.low.assign(d, 0);
                cell.high.assign(d, 1000000);

                auto t0 = std::chrono::high_resolution_clock::now();
                Int128 v_tensor = (d == 5 ? evaluate_tensor_clean_cell_d5(cell, boxes) : evaluate_tensor_clean_cell_d6(cell, boxes));
                auto t1 = std::chrono::high_resolution_clock::now();
                tensor_tot_us += std::chrono::duration<double, std::micro>(t1 - t0).count();

                auto t2 = std::chrono::high_resolution_clock::now();
                Int128 v_sweep = exact_kmp_sweep(boxes, d);
                auto t3 = std::chrono::high_resolution_clock::now();
                sweep_tot_us += std::chrono::duration<double, std::micro>(t3 - t2).count();

                if (v_tensor != v_sweep) all_match = false;
            }

            double t_avg = tensor_tot_us / ITERS;
            double s_avg = sweep_tot_us / ITERS;
            double sp = s_avg / std::max(0.1, t_avg);

            std::cout << std::setw(3) << n << " | "
                      << std::setw(11) << std::fixed << std::setprecision(2) << t_avg << " | "
                      << std::setw(10) << std::fixed << std::setprecision(2) << s_avg << " | "
                      << std::setw(6) << std::fixed << std::setprecision(2) << sp << "x | "
                      << (all_match ? "PASS" : "FAIL") << std::endl;

            if (t_avg > 50000.0) break; // Don't run too long
        }
    }
    return 0;
}
