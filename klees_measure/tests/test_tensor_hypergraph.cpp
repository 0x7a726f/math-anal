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

void test_hypergraph_d4() {
    std::cout << "[Test 1/4] Running d=4 Mixed (2,3)-Slab Hypergraph Tensor Contraction..." << std::endl;
    FastRng rng(4242);
    const int d = 4;
    int passed = 0;

    for (int iter = 0; iter < 120; ++iter) {
        int64_t M = 40;
        Box cell;
        cell.low.assign(d, 0);
        cell.high.assign(d, M);

        int n_boxes = (int)rng.next_range(3, 12);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);

            // Choose slab degree k in {1, 2, 3}
            int slab_deg = (int)rng.next_range(1, 3);
            std::vector<int> dims(d);
            for (int i = 0; i < d; ++i) dims[i] = i;
            // Fisher-Yates shuffle
            for (int i = 0; i < d; ++i) {
                int j = i + (rng.next() % (d - i));
                std::swap(dims[i], dims[j]);
            }

            for (int s = 0; s < slab_deg; ++s) {
                int dim = dims[s];
                int64_t u = rng.next_range(0, M - 5);
                int64_t v = rng.next_range(u + 2, M);
                b.low[dim] = u;
                b.high[dim] = v;
            }
            boxes.push_back(b);
        }

        assert(is_hypergraph_clean_cell(cell, boxes));

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 t4_vol = evaluate_tensor_hypergraph_d4(cell, boxes);
        Int128 disp_vol = evaluate_tensor_clean_cell(cell, boxes);

        if (gt_vol != t4_vol || gt_vol != disp_vol) {
            std::cerr << "FAIL at d=4 hypergraph iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " T4=" << t4_vol.to_string()
                      << " DISP=" << disp_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " d=4 mixed (2,3)-slab cells verified with 100% exact volume!" << std::endl;
}

void test_hypergraph_d5() {
    std::cout << "[Test 2/4] Running d=5 Mixed (2,3)-Slab Hypergraph Tensor Contraction..." << std::endl;
    FastRng rng(55555);
    const int d = 5;
    int passed = 0;

    for (int iter = 0; iter < 100; ++iter) {
        int64_t M = 30;
        Box cell;
        cell.low.assign(d, 0);
        cell.high.assign(d, M);

        int n_boxes = (int)rng.next_range(3, 10);
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
                int64_t u = rng.next_range(0, M - 5);
                int64_t v = rng.next_range(u + 2, M);
                b.low[dim] = u;
                b.high[dim] = v;
            }
            boxes.push_back(b);
        }

        assert(is_hypergraph_clean_cell(cell, boxes));

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 t5_vol = evaluate_tensor_hypergraph_d5(cell, boxes);
        Int128 disp_vol = evaluate_tensor_clean_cell(cell, boxes);

        if (gt_vol != t5_vol || gt_vol != disp_vol) {
            std::cerr << "FAIL at d=5 hypergraph iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " T5=" << t5_vol.to_string()
                      << " DISP=" << disp_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " d=5 mixed (2,3)-slab cells verified with 100% exact volume!" << std::endl;
}

void test_corner_2cut_equivalence() {
    std::cout << "[Test 3/4] Running Joint Orthogonal Corner 2-Cut Equivalence..." << std::endl;
    FastRng rng(9876);
    int passed = 0;

    for (int iter = 0; iter < 50; ++iter) {
        int d = 4;
        int64_t M = 1000;
        int n_boxes = 16;
        auto boxes = generate_trellis_weave(n_boxes, d, 200 + iter);

        StructuralCounters c_default, c_corner;
        SplitPolicy p_default;
        p_default.use_corner_2cuts = false;

        SplitPolicy p_corner;
        p_corner.use_corner_2cuts = true;
        p_corner.corner_2cut_face_threshold = 0; // Force corner 2-cuts at all internal nodes!

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 v_default = KMPSolver::solve(boxes, c_default, p_default);
        Int128 v_corner = KMPSolver::solve(boxes, c_corner, p_corner);

        if (gt_vol != v_corner || v_default != v_corner) {
            std::cerr << "FAIL at Corner 2-Cut iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " Default=" << v_default.to_string()
                      << " Corner=" << v_corner.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " Corner 2-Cut instances verified with 100% exact volume!" << std::endl;
}

void test_hypergraph_performance() {
    std::cout << "[Test 4/4] Running Mixed (2,3)-Slab Hypergraph Throughput Benchmark..." << std::endl;
    FastRng rng(777);
    int d = 4;
    int64_t M = 500;
    Box cell;
    cell.low.assign(d, 0);
    cell.high.assign(d, M);

    std::vector<std::vector<Box>> instances;
    for (int i = 0; i < 100; ++i) {
        int n_boxes = 10;
        std::vector<Box> boxes;
        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);
            int slab_deg = (k % 3) + 1; // mixture of 1, 2, 3
            for (int s = 0; s < slab_deg; ++s) {
                int dim = s;
                int64_t u = rng.next_range(0, M - 50);
                int64_t v = rng.next_range(u + 10, M);
                b.low[dim] = u;
                b.high[dim] = v;
            }
            boxes.push_back(b);
        }
        instances.push_back(boxes);
    }

    int runs = 1000;
    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 dummy(0);
    for (int r = 0; r < runs; ++r) {
        dummy += evaluate_tensor_hypergraph_d4(cell, instances[r % 100]);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double us_per_call = (ms * 1000.0) / runs;

    std::cout << "  -> d=4 Hypergraph Benchmark: " << runs << " runs in " << ms << " ms ("
              << us_per_call << " us/call)." << std::endl;
    std::cout << "  -> Throughput: " << (runs / (ms / 1000.0)) << " evaluations/second." << std::endl;
}

int main() {
    std::cout << "=================================================================" << std::endl;
    std::cout << " KMP Hypergraph Tensor Contraction & Corner 2-Cut Verification   " << std::endl;
    std::cout << "=================================================================" << std::endl;

    test_hypergraph_d4();
    test_hypergraph_d5();
    test_corner_2cut_equivalence();
    test_hypergraph_performance();

    std::cout << "\n>>> ALL HYPERGRAPH TENSOR & CORNER 2-CUT TESTS PASSED (100% EXACT)! <<<" << std::endl;
    return 0;
}
