#include <iostream>
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"
#include "../harness/generators.hpp"
#include "../solver/tensor_clean_cell.hpp"
#include "../solver/split_policy.hpp"
#include "../solver/champion_policy.hpp"
#include "../solver/kmp_solver.hpp"

using namespace kmp;

void test_hypergraph_d6() {
    std::cout << "[Test 1/3] Running d=6 Mixed (2,3)-Slab Hypergraph Tensor Contraction..." << std::endl;
    FastRng rng(6666);
    const int d = 6;
    int passed = 0;

    for (int iter = 0; iter < 80; ++iter) {
        int64_t M = 20;
        Box cell;
        cell.low.assign(d, 0);
        cell.high.assign(d, M);

        int n_boxes = (int)rng.next_range(3, 8);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);

            int slab_deg = (int)rng.next_range(1, 3);
            std::vector<int> dims(d);
            for (int i = 0; i < d; ++i) dims[i] = i;
            for (int i = 0; i < d; ++i) {
                int j = i + (rng.next() % (d - i));
                std::swap(dims[i], dims[j]);
            }

            for (int s = 0; s < slab_deg; ++s) {
                int dim = dims[s];
                int64_t u = rng.next_range(0, M - 4);
                int64_t v = rng.next_range(u + 2, M);
                b.low[dim] = u;
                b.high[dim] = v;
            }
            boxes.push_back(b);
        }

        assert(is_hypergraph_clean_cell(cell, boxes));

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 t6_vol = evaluate_tensor_hypergraph_d6(cell, boxes);
        Int128 disp_vol = evaluate_tensor_clean_cell(cell, boxes);

        if (gt_vol != t6_vol || gt_vol != disp_vol) {
            std::cerr << "FAIL at d=6 hypergraph iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " T6=" << t6_vol.to_string()
                      << " DISP=" << disp_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " d=6 mixed (2,3)-slab cells verified with 100% exact volume!" << std::endl;
}

void test_hsah_and_solver_d6() {
    std::cout << "[Test 2/3] Running KMPSolver with HSAH Metric on 6D Box Sets..." << std::endl;
    FastRng rng(12345);
    const int d = 6;
    int passed = 0;

    SplitPolicy policy;

    for (int iter = 0; iter < 30; ++iter) {
        int64_t M = 50;
        int n_boxes = (int)rng.next_range(4, 12);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);

            // Mixture of 1, 2, 3, and 4-slabs
            int slab_deg = (int)rng.next_range(1, 4);
            std::vector<int> dims(d);
            for (int i = 0; i < d; ++i) dims[i] = i;
            for (int i = 0; i < d; ++i) {
                int j = i + (rng.next() % (d - i));
                std::swap(dims[i], dims[j]);
            }

            for (int s = 0; s < slab_deg; ++s) {
                int dim = dims[s];
                int64_t u = rng.next_range(0, M - 6);
                int64_t v = rng.next_range(u + 3, M);
                b.low[dim] = u;
                b.high[dim] = v;
            }
            boxes.push_back(b);
        }

        StructuralCounters counters;
        Int128 solver_vol = KMPSolver::solve(boxes, counters, policy);
        Int128 gt_vol = exact_kmp_sweep(boxes, d);

        if (solver_vol != gt_vol) {
            std::cerr << "FAIL at HSAH d=6 iter " << iter
                      << ": Solver=" << solver_vol.to_string()
                      << " GT=" << gt_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " 6D solver runs with HSAH verified with 100% exact volume!" << std::endl;
}

void test_hypergraph_d6_benchmark() {
    std::cout << "[Test 3/3] Running d=6 Hypergraph Throughput Benchmark..." << std::endl;
    FastRng rng(999);
    const int d = 6;
    int64_t M = 100;
    Box cell;
    cell.low.assign(d, 0);
    cell.high.assign(d, M);

    std::vector<std::vector<Box>> instances;
    for (int i = 0; i < 50; ++i) {
        int n_boxes = 6;
        std::vector<Box> boxes;
        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);
            int slab_deg = (k % 3) + 1; // 1, 2, 3
            for (int s = 0; s < slab_deg; ++s) {
                int dim = s;
                int64_t u = rng.next_range(0, M - 20);
                int64_t v = rng.next_range(u + 5, M);
                b.low[dim] = u;
                b.high[dim] = v;
            }
            boxes.push_back(b);
        }
        instances.push_back(boxes);
    }

    int runs = 200;
    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 dummy(0);
    for (int r = 0; r < runs; ++r) {
        dummy += evaluate_tensor_hypergraph_d6(cell, instances[r % 50]);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double us_per_call = (ms * 1000.0) / runs;

    std::cout << "  -> d=6 Hypergraph Benchmark: " << runs << " runs in " << ms << " ms ("
              << us_per_call << " us/call)." << std::endl;
    std::cout << "  -> Throughput: " << (runs / (ms / 1000.0)) << " evaluations/second." << std::endl;
}

int main() {
    std::cout << "=================================================================" << std::endl;
    std::cout << " KMP 6D Hypergraph Tensor Contraction & HSAH Verification Suite  " << std::endl;
    std::cout << "=================================================================" << std::endl;

    test_hypergraph_d6();
    test_hsah_and_solver_d6();
    test_hypergraph_d6_benchmark();

    std::cout << "\n>>> ALL 6D HYPERGRAPH TENSOR & HSAH TESTS PASSED (100% EXACT)! <<<" << std::endl;
    return 0;
}
