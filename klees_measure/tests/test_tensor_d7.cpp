#include <iostream>
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"
#include "../harness/generators.hpp"
#include "../solver/tensor_clean_cell.hpp"

using namespace kmp;

void test_d7_edge_cases() {
    std::cout << "[Test 1/5] Running d=7 Clean-Cell Edge Cases..." << std::endl;
    int d = 7;
    int64_t M = 100;
    Box cell;
    cell.low.assign(d, 0);
    cell.high.assign(d, M);

    // Case 1: Empty boxes
    {
        std::vector<Box> empty_boxes;
        Int128 gt = exact_kmp_sweep(empty_boxes, d);
        Int128 t_d7 = evaluate_tensor_clean_cell_d7(cell, empty_boxes);
        Int128 t_gen = evaluate_tensor_clean_cell_general(cell, empty_boxes);
        assert(gt == Int128(0));
        assert(t_d7 == Int128(0));
        assert(t_gen == Int128(0));
    }

    // Case 2: Full covering box
    {
        std::vector<Box> boxes;
        Box b = cell;
        boxes.push_back(b);
        Int128 gt = exact_kmp_sweep(boxes, d);
        Int128 t_d7 = evaluate_tensor_clean_cell_d7(cell, boxes);
        Int128 t_gen = evaluate_tensor_clean_cell_general(cell, boxes);
        assert(gt == cell.volume());
        assert(t_d7 == cell.volume());
        assert(t_gen == cell.volume());
    }

    // Case 3: Pure 1-slabs
    {
        std::vector<Box> boxes;
        for (int i = 0; i < d; ++i) {
            Box b = cell;
            b.low[i] = 10 * (i + 1);
            b.high[i] = 10 * (i + 1) + 5;
            boxes.push_back(b);
        }
        Int128 gt = exact_kmp_sweep(boxes, d);
        Int128 t_d7 = evaluate_tensor_clean_cell_d7(cell, boxes);
        Int128 t_gen = evaluate_tensor_clean_cell_general(cell, boxes);
        assert(gt == t_d7);
        assert(gt == t_gen);
    }

    // Case 4: Disjoint 2-slabs
    {
        std::vector<Box> boxes;
        Box b1 = cell;
        b1.low[0] = 0; b1.high[0] = 10;
        b1.low[1] = 0; b1.high[1] = 10;
        boxes.push_back(b1);

        Box b2 = cell;
        b2.low[2] = 20; b2.high[2] = 30;
        b2.low[3] = 20; b2.high[3] = 30;
        boxes.push_back(b2);

        Int128 gt = exact_kmp_sweep(boxes, d);
        Int128 t_d7 = evaluate_tensor_clean_cell_d7(cell, boxes);
        Int128 t_gen = evaluate_tensor_clean_cell_general(cell, boxes);
        assert(gt == t_d7);
        assert(gt == t_gen);
    }

    std::cout << "  -> PASSED: All d=7 Edge Cases Verified!" << std::endl;
}

void test_d7_random_clean_cells() {
    std::cout << "[Test 2/5] Running d=7 Randomized Clean-Cell Suite against Exact Sweep..." << std::endl;
    FastRng rng(777);
    const int d = 7;
    int passed = 0;

    for (int iter = 0; iter < 100; ++iter) {
        int64_t M = 40;
        Box cell;
        cell.low.assign(d, 0);
        cell.high.assign(d, M);

        int n_boxes = (int)rng.next_range(2, 10);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);

            int type = (int)(rng.next() % 4);
            if (type == 0) {
                // 1-slab
                int dim = (int)(rng.next() % d);
                int64_t u = rng.next_range(0, M - 5);
                int64_t v = rng.next_range(u + 2, M);
                b.low[dim] = u;
                b.high[dim] = v;
            } else {
                // 2-slab
                int dim1 = (int)(rng.next() % d);
                int dim2 = (dim1 + 1 + (rng.next() % (d - 1))) % d;
                int64_t u1 = rng.next_range(0, M - 5);
                int64_t v1 = rng.next_range(u1 + 2, M);
                int64_t u2 = rng.next_range(0, M - 5);
                int64_t v2 = rng.next_range(u2 + 2, M);
                b.low[dim1] = u1;
                b.high[dim1] = v1;
                b.low[dim2] = u2;
                b.high[dim2] = v2;
            }
            boxes.push_back(b);
        }

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 d7_vol = evaluate_tensor_clean_cell_d7(cell, boxes);
        Int128 gen_vol = evaluate_tensor_clean_cell_general(cell, boxes);

        if (gt_vol != d7_vol || gt_vol != gen_vol) {
            std::cerr << "FAIL at d=7 iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " D7=" << d7_vol.to_string()
                      << " GEN=" << gen_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " randomized d=7 clean cells verified with 100% exact volume!" << std::endl;
}

void test_d7_trellis_weaves() {
    std::cout << "[Test 3/5] Running d=7 Trellis Weave Adversarial Suite..." << std::endl;
    FastRng rng(99999);
    const int d = 7;
    int passed = 0;

    // Test across various box counts with scaled coordinate bound M = 10,000
    for (int iter = 0; iter < 40; ++iter) {
        int64_t M = 10000;
        Box cell;
        cell.low.assign(d, 0);
        cell.high.assign(d, M);

        int n_boxes = (int)rng.next_range(4, 14);
        std::vector<Box> boxes;
        std::vector<std::pair<int, int>> pairs;
        for (int i = 0; i < d; ++i) {
            for (int j = i + 1; j < d; ++j) {
                pairs.push_back({i, j});
            }
        }

        for (int k = 0; k < n_boxes; ++k) {
            auto p = pairs[(k + iter) % pairs.size()];
            int dim1 = p.first;
            int dim2 = p.second;

            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);

            int64_t u1 = rng.next_range(0, M - 1000);
            int64_t len1 = rng.next_range(500, 2000);
            b.low[dim1] = u1;
            b.high[dim1] = std::min(M, u1 + len1);

            int64_t u2 = rng.next_range(0, M - 1000);
            int64_t len2 = rng.next_range(500, 2000);
            b.low[dim2] = u2;
            b.high[dim2] = std::min(M, u2 + len2);

            boxes.push_back(b);
        }

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 d7_vol = evaluate_tensor_clean_cell_d7(cell, boxes);
        Int128 gen_vol = evaluate_tensor_clean_cell_general(cell, boxes);

        if (gt_vol != d7_vol || gt_vol != gen_vol) {
            std::cerr << "FAIL at d=7 Trellis iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " D7=" << d7_vol.to_string()
                      << " GEN=" << gen_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " d=7 Trellis Weave instances verified with 100% exact volume!" << std::endl;
}

void test_general_d8() {
    std::cout << "[Test 4/5] Running evaluate_tensor_clean_cell_general for d=8..." << std::endl;
    FastRng rng(8888);
    const int d = 8;
    int passed = 0;

    for (int iter = 0; iter < 30; ++iter) {
        int64_t M = 30;
        Box cell;
        cell.low.assign(d, 0);
        cell.high.assign(d, M);

        int n_boxes = (int)rng.next_range(2, 6);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);

            int dim1 = (int)(rng.next() % d);
            int dim2 = (dim1 + 1 + (rng.next() % (d - 1))) % d;
            int64_t u1 = rng.next_range(0, M - 5);
            int64_t v1 = rng.next_range(u1 + 2, M);
            int64_t u2 = rng.next_range(0, M - 5);
            int64_t v2 = rng.next_range(u2 + 2, M);
            b.low[dim1] = u1;
            b.high[dim1] = v1;
            b.low[dim2] = u2;
            b.high[dim2] = v2;
            boxes.push_back(b);
        }

        Int128 gt_vol = exact_kmp_sweep(boxes, d);
        Int128 gen_vol = evaluate_tensor_clean_cell_general(cell, boxes);
        Int128 disp_vol = evaluate_tensor_clean_cell(cell, boxes);

        if (gt_vol != gen_vol || gt_vol != disp_vol) {
            std::cerr << "FAIL at d=8 iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " GEN=" << gen_vol.to_string()
                      << " DISP=" << disp_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " d=8 instances verified with 100% exact volume!" << std::endl;
}

void test_d7_benchmark() {
    std::cout << "[Test 5/5] Running d=7 Performance & Throughput Benchmark..." << std::endl;
    FastRng rng(42);
    const int d = 7;
    int64_t M = 1000;
    Box cell;
    cell.low.assign(d, 0);
    cell.high.assign(d, M);

    // Pre-generate 100 clean cell instances
    std::vector<std::vector<Box>> test_instances;
    for (int i = 0; i < 100; ++i) {
        int n_boxes = 8;
        std::vector<Box> boxes;
        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(d, 0);
            b.high.assign(d, M);
            int dim1 = (int)(rng.next() % d);
            int dim2 = (dim1 + 1 + (rng.next() % (d - 1))) % d;
            int64_t u1 = rng.next_range(0, M - 100);
            int64_t v1 = rng.next_range(u1 + 20, M);
            int64_t u2 = rng.next_range(0, M - 100);
            int64_t v2 = rng.next_range(u2 + 20, M);
            b.low[dim1] = u1;
            b.high[dim1] = v1;
            b.low[dim2] = u2;
            b.high[dim2] = v2;
            boxes.push_back(b);
        }
        test_instances.push_back(boxes);
    }

    // Benchmark evaluate_tensor_clean_cell_d7
    int benchmark_runs = 500;
    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 dummy(0);
    for (int r = 0; r < benchmark_runs; ++r) {
        dummy += evaluate_tensor_clean_cell_d7(cell, test_instances[r % 100]);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double per_call_us = (elapsed_ms * 1000.0) / benchmark_runs;

    std::cout << "  -> Benchmark: " << benchmark_runs << " evaluations completed in "
              << elapsed_ms << " ms (" << per_call_us << " us / evaluation)." << std::endl;
    std::cout << "  -> Throughput: " << (benchmark_runs / (elapsed_ms / 1000.0))
              << " clean-cell evaluations / second." << std::endl;
}

int main() {
    std::cout << "=================================================================" << std::endl;
    std::cout << " KMP Higher-Dimensional Tensor Contraction Engine (d=7 & General) " << std::endl;
    std::cout << "=================================================================" << std::endl;

    test_d7_edge_cases();
    test_d7_random_clean_cells();
    test_d7_trellis_weaves();
    test_general_d8();
    test_d7_benchmark();

    std::cout << "\n>>> ALL TEST SUITES PASSED WITH 100% EXACT VOLUME VERIFICATION! <<<" << std::endl;
    return 0;
}
