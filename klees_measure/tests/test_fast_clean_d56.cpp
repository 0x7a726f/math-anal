#include <iostream>
#include <chrono>
#include <iomanip>
#include "harness/types.hpp"
#include "harness/generators.hpp"
#include "solver/kmp_solver.hpp"
#include "solver/champion_policy.hpp"
#include "solver/baseline_policy.hpp"

using namespace kmp;

struct OptimalChampionPolicy : public ChampionPolicy {
    bool should_terminate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        uint64_t active_d2_faces
    ) const {
        if (boxes.empty()) return true;
        int d = cell.dim();
        int threshold = thresh_d3;
        if (d >= 6) threshold = thresh_d6; // 18
        else if (d == 5) threshold = thresh_d5; // 24
        else if (d == 4) threshold = thresh_d4; // 48
        if ((int)boxes.size() <= threshold) return true;
        if (active_d2_faces == 0) return true;
        if (d == 3 && is_clean_cell(cell, boxes)) return true;
        return false;
    }

    Int128 evaluate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        int d
    ) const {
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();
        if (d <= 6 && is_clean_cell(cell, boxes)) {
            return evaluate_tensor_clean_cell(cell, boxes);
        }
        return exact_kmp_sweep(boxes, d);
    }
};

int main() {
    OptimalChampionPolicy champ;
    BaselinePolicy base;
    std::cout << "Testing OptimalChampionPolicy across d in {3, 4, 5, 6} on Trellis..." << std::endl;
    for (int d : {3, 4, 5, 6}) {
        for (int n : {64, 128, 256}) {
            auto boxes = generate_trellis_weave(n, d, 42);
            StructuralCounters c_c, c_b;

            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 v_c = KMPSolver::solve(boxes, c_c, champ);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms_c = std::chrono::duration<double, std::milli>(t1 - t0).count();

            auto t2 = std::chrono::high_resolution_clock::now();
            Int128 v_b = KMPSolver::solve(boxes, c_b, base);
            auto t3 = std::chrono::high_resolution_clock::now();
            double ms_b = std::chrono::duration<double, std::milli>(t3 - t2).count();

            bool pass = (v_c == v_b);
            double sp = ms_b / std::max(0.01, ms_c);
            double face_red = (double)c_b.total_d2_faces_processed / std::max(1ULL, c_c.total_d2_faces_processed);

            std::cout << "d=" << d << " n=" << std::setw(3) << n << " | "
                      << "Champ: " << std::setw(7) << std::fixed << std::setprecision(2) << ms_c << " ms | "
                      << "Base: " << std::setw(7) << std::fixed << std::setprecision(2) << ms_b << " ms | "
                      << "Speedup: " << std::setw(5) << std::fixed << std::setprecision(2) << sp << "x | "
                      << "FaceRed: " << std::setw(5) << std::fixed << std::setprecision(2) << face_red << "x | "
                      << (pass ? "PASS" : "FAIL") << std::endl;
        }
    }
    return 0;
}
