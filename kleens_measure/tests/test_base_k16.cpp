#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "../solver/kmp_solver.hpp"

using namespace kmp;

struct TestBaseline {
    int get_threshold(int d) const {
        if (d >= 6) return 16;
        if (d == 5) return 20;
        if (d == 4) return 24;
        return 32;
    }
    int select_split_axis(const Box& cell, const std::vector<Box>&, const std::vector<std::vector<uint32_t>>&, int last_dim) const {
        return (last_dim + 1) % cell.dim();
    }
    int64_t select_cut_coordinate(int axis, const Box& cell, const std::vector<Box>& boxes) const {
        int64_t c_low = cell.low[axis], c_high = cell.high[axis];
        std::vector<int64_t> coords;
        for (const auto& b : boxes) {
            if (b.low[axis] > c_low && b.low[axis] < c_high) coords.push_back(b.low[axis]);
            if (b.high[axis] > c_low && b.high[axis] < c_high) coords.push_back(b.high[axis]);
        }
        if (coords.empty()) return c_low + (c_high - c_low) / 2;
        std::sort(coords.begin(), coords.end());
        return coords[coords.size() / 2];
    }
    bool should_terminate_leaf(const Box& cell, const std::vector<Box>& boxes, uint64_t) const {
        return (int)boxes.size() <= get_threshold(cell.dim());
    }
    Int128 evaluate_leaf(const Box& cell, const std::vector<Box>& boxes, int d) const {
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();
        return exact_kmp_sweep(boxes, d);
    }
};

int main() {
    TestBaseline base;
    std::cout << "Testing Baseline (cyclic + median) with threshold 16/20..." << std::endl;
    for (int d : {5, 6}) {
        for (int n : {64, 128, 256, 512, 1024}) {
            auto boxes = generate_trellis_weave(n, d, 42);
            StructuralCounters counters;
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 vol = KMPSolver::solve(boxes, counters, base);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "d=" << d << " n=" << std::setw(4) << n << " | ms=" << std::setw(8) << ms << " calls=" << counters.recursive_calls << " faces=" << counters.total_d2_faces_processed << std::endl;
            if (ms > 4000.0) break;
        }
    }
    return 0;
}
