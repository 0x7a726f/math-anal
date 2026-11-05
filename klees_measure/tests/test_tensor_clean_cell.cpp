#include <iostream>
#include <cassert>
#include <vector>
#include <random>
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"
#include "../harness/generators.hpp"
#include "../solver/tensor_clean_cell.hpp"

using namespace kmp;

void test_d3_random_and_adversarial() {
    std::cout << "[C++ Test] Running d=3 Clean-Cell Tensor Contraction Suite..." << std::endl;
    FastRng rng(42);
    int passed = 0;

    for (int iter = 0; iter < 200; ++iter) {
        int64_t M = 100;
        Box cell;
        cell.low.assign(3, 0);
        cell.high.assign(3, M);

        int n_boxes = (int)rng.next_range(2, 16);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(3, 0);
            b.high.assign(3, M);

            int type = (int)(rng.next() % 4);
            if (type == 0) {
                // 1-slab
                int dim = (int)(rng.next() % 3);
                int64_t u = rng.next_range(0, M - 5);
                int64_t v = rng.next_range(u + 2, M);
                b.low[dim] = u;
                b.high[dim] = v;
            } else {
                // 2-slab
                int dim1 = (int)(rng.next() % 3);
                int dim2 = (dim1 + 1 + (rng.next() % 2)) % 3;
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

        Int128 gt_vol = exact_kmp_sweep(boxes, 3);
        Int128 tensor_vol = evaluate_tensor_clean_cell_d3(cell, boxes);

        if (gt_vol != tensor_vol) {
            std::cerr << "FAIL at d=3 iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " Tensor=" << tensor_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    // Adversarial Trellis Weaves d=3
    for (int iter = 0; iter < 50; ++iter) {
        int64_t M = 1000000;
        Box cell;
        cell.low.assign(3, 0);
        cell.high.assign(3, M);
        auto boxes = generate_trellis_weave(16, 3, 100 + iter);
        Int128 gt_vol = exact_kmp_sweep(boxes, 3);
        Int128 tensor_vol = evaluate_tensor_clean_cell_d3(cell, boxes);
        assert(gt_vol == tensor_vol);
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " instances verified with 100% exact volume!" << std::endl;
}

void test_d4_random_and_adversarial() {
    std::cout << "[C++ Test] Running d=4 Clean-Cell Tensor Contraction Suite..." << std::endl;
    FastRng rng(12345);
    int passed = 0;

    for (int iter = 0; iter < 200; ++iter) {
        int64_t M = 50;
        Box cell;
        cell.low.assign(4, 0);
        cell.high.assign(4, M);

        int n_boxes = (int)rng.next_range(2, 14);
        std::vector<Box> boxes;

        for (int k = 0; k < n_boxes; ++k) {
            Box b;
            b.low.assign(4, 0);
            b.high.assign(4, M);

            int type = (int)(rng.next() % 5);
            if (type == 0) {
                // 1-slab
                int dim = (int)(rng.next() % 4);
                int64_t u = rng.next_range(0, M - 5);
                int64_t v = rng.next_range(u + 2, M);
                b.low[dim] = u;
                b.high[dim] = v;
            } else {
                // 2-slab
                int dim1 = (int)(rng.next() % 4);
                int dim2 = (dim1 + 1 + (rng.next() % 3)) % 4;
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

        Int128 gt_vol = exact_kmp_sweep(boxes, 4);
        Int128 tensor_vol = evaluate_tensor_clean_cell_d4(cell, boxes);

        if (gt_vol != tensor_vol) {
            std::cerr << "FAIL at d=4 iter " << iter
                      << ": GT=" << gt_vol.to_string()
                      << " Tensor=" << tensor_vol.to_string() << std::endl;
            exit(1);
        }
        passed++;
    }

    // Adversarial Trellis Weaves d=4
    for (int iter = 0; iter < 50; ++iter) {
        int64_t M = 1000000;
        Box cell;
        cell.low.assign(4, 0);
        cell.high.assign(4, M);
        auto boxes = generate_trellis_weave(16, 4, 200 + iter);
        Int128 gt_vol = exact_kmp_sweep(boxes, 4);
        Int128 tensor_vol = evaluate_tensor_clean_cell_d4(cell, boxes);
        assert(gt_vol == tensor_vol);
        passed++;
    }

    // Edge Cases:
    // 1. 0-slab covering cell
    {
        Box cell; cell.low.assign(4, 0); cell.high.assign(4, 100);
        Box b0; b0.low.assign(4, 0); b0.high.assign(4, 100);
        std::vector<Box> b = {b0};
        assert(evaluate_tensor_clean_cell_d4(cell, b) == cell.volume());
        passed++;
    }

    // 2. Empty box set
    {
        Box cell; cell.low.assign(4, 0); cell.high.assign(4, 100);
        std::vector<Box> b = {};
        assert(evaluate_tensor_clean_cell_d4(cell, b) == Int128(0));
        passed++;
    }

    // 3. Only 1-slabs
    {
        Box cell; cell.low.assign(4, 0); cell.high.assign(4, 100);
        Box b1; b1.low = {10, 0, 0, 0}; b1.high = {40, 100, 100, 100};
        Box b2; b2.low = {0, 20, 0, 0}; b2.high = {100, 60, 100, 100};
        std::vector<Box> b = {b1, b2};
        Int128 gt = exact_kmp_sweep(b, 4);
        Int128 tensor = evaluate_tensor_clean_cell_d4(cell, b);
        assert(gt == tensor);
        passed++;
    }

    std::cout << "  -> PASSED: " << passed << " instances verified with 100% exact volume!" << std::endl;
}

int main() {
    std::cout << "================================================================" << std::endl;
    std::cout << "C++ Tensor Clean-Cell Verification Test Suite" << std::endl;
    std::cout << "================================================================" << std::endl;
    test_d3_random_and_adversarial();
    test_d4_random_and_adversarial();
    std::cout << "================================================================" << std::endl;
    std::cout << "ALL C++ TENSOR NETWORK VERIFICATION TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "================================================================" << std::endl;
    return 0;
}
