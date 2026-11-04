#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"
#include "../solver/split_policy.hpp"

using namespace kmp;

struct BaselineH00Policy {
    std::string id = "H-00: Baseline cyclic Chan";
    int leaf_box_threshold = 1;
    uint64_t leaf_d2_face_threshold = 0;
    double cut_plane_ratio = 0.50;

    int select_split_axis(const Box& cell, const std::vector<Box>& boxes, const std::vector<std::vector<uint32_t>>& d2_face_matrix, int last_split_dim) const {
        return (last_split_dim + 1) % cell.dim();
    }

    int64_t select_cut_coordinate(int axis, const Box& cell, const std::vector<Box>& boxes) const {
        int64_t c_low = cell.low[axis];
        int64_t c_high = cell.high[axis];
        std::vector<int64_t> coords;
        for (const auto& b : boxes) {
            if (b.low[axis] > c_low && b.low[axis] < c_high) coords.push_back(b.low[axis]);
            if (b.high[axis] > c_low && b.high[axis] < c_high) coords.push_back(b.high[axis]);
        }
        if (coords.empty()) return c_low + (c_high - c_low) / 2;
        std::sort(coords.begin(), coords.end());
        return coords[coords.size() / 2];
    }

    bool should_terminate_leaf(const Box& cell, const std::vector<Box>& boxes, uint64_t active_d2_faces) const {
        return boxes.size() <= 1;
    }

    Int128 evaluate_leaf(const Box& cell, const std::vector<Box>& boxes, int d) const {
        if (boxes.empty()) return Int128(0);
        return boxes[0].volume();
    }
};

int main() {
    SplitPolicy champion_policy = get_split_policy();
    BaselineH00Policy baseline_policy;

    std::cout << "Testing H-00 baseline vs Champion H-51..." << std::endl;
    for (int d : {3, 4}) {
        for (int n : {64, 128, 256, 512, 1024}) {
            auto boxes = generate_trellis_weave(n, d, 42);

            StructuralCounters c_base;
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 v_base = KMPSolver::solve(boxes, c_base, baseline_policy);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms_base = std::chrono::duration<double, std::milli>(t1 - t0).count();

            StructuralCounters c_champ;
            auto t2 = std::chrono::high_resolution_clock::now();
            Int128 v_champ = KMPSolver::solve(boxes, c_champ, champion_policy);
            auto t3 = std::chrono::high_resolution_clock::now();
            double ms_champ = std::chrono::duration<double, std::milli>(t3 - t2).count();

            bool match = (v_base == v_champ);
            std::cout << "d=" << d << " n=" << std::setw(4) << n
                      << " | Base: " << std::setw(7) << ms_base << " ms (calls=" << std::setw(7) << c_base.recursive_calls << ", faces=" << std::setw(8) << c_base.total_d2_faces_processed << ")"
                      << " | Champ: " << std::setw(7) << ms_champ << " ms (calls=" << std::setw(5) << c_champ.recursive_calls << ", faces=" << std::setw(7) << c_champ.total_d2_faces_processed << ")"
                      << " | Speedup: " << std::setw(5) << (ms_base / ms_champ) << "x"
                      << " | Match: " << (match ? "YES" : "NO")
                      << std::endl;
        }
    }
    return 0;
}
