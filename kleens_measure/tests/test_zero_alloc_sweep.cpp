#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

inline Int128 fast_stack_sweep_nd(const SweepStackBox* boxes, size_t n, int d) {
    if (n == 0 || d <= 0) return Int128(0);
    if (n == 1) return boxes[0].volume();

    if (d == 1) {
        std::pair<int64_t, int64_t> stack_buf[128];
        for (size_t i = 0; i < n; ++i) stack_buf[i] = {boxes[i].low[0], boxes[i].high[0]};
        return stack_sweep_1d(stack_buf, n);
    }
    if (d == 2) {
        return stack_sweep_2d(boxes, n);
    }

    int last_dim = d - 1;
    int64_t stack_coords[96];
    size_t n_coords = n * 2;
    for (size_t i = 0; i < n; ++i) {
        stack_coords[2 * i] = boxes[i].low[last_dim];
        stack_coords[2 * i + 1] = boxes[i].high[last_dim];
    }
    std::sort(stack_coords, stack_coords + n_coords);
    size_t unique_coords = std::unique(stack_coords, stack_coords + n_coords) - stack_coords;

    Int128 total_volume(0);
    SweepStackBox stack_sub_boxes[48];

    for (size_t k = 0; k + 1 < unique_coords; ++k) {
        int64_t slab_l = stack_coords[k];
        int64_t slab_r = stack_coords[k + 1];
        if (slab_r <= slab_l) continue;

        size_t sub_count = 0;
        for (size_t i = 0; i < n; ++i) {
            if (boxes[i].low[last_dim] <= slab_l && boxes[i].high[last_dim] >= slab_r) {
                SweepStackBox& sb = stack_sub_boxes[sub_count++];
                sb.d = last_dim;
                for (int j = 0; j < last_dim; ++j) {
                    sb.low[j] = boxes[i].low[j];
                    sb.high[j] = boxes[i].high[j];
                }
            }
        }

        if (sub_count > 0) {
            Int128 cross_vol = fast_stack_sweep_nd(stack_sub_boxes, sub_count, last_dim);
            total_volume += cross_vol * (slab_r - slab_l);
        }
    }
    return total_volume;
}

int main() {
    auto boxes = generate_trellis_weave(20, 6, 42);
    std::vector<SweepStackBox> sboxes(20);
    for (int i = 0; i < 20; ++i) {
        sboxes[i].d = 6;
        for (int j = 0; j < 6; ++j) {
            sboxes[i].low[j] = boxes[i].low[j];
            sboxes[i].high[j] = boxes[i].high[j];
        }
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 v1 = stack_sweep_nd(sboxes.data(), 20, 6);
    auto t1 = std::chrono::high_resolution_clock::now();
    double us1 = std::chrono::duration<double, std::micro>(t1 - t0).count();

    auto t2 = std::chrono::high_resolution_clock::now();
    Int128 v2 = fast_stack_sweep_nd(sboxes.data(), 20, 6);
    auto t3 = std::chrono::high_resolution_clock::now();
    double us2 = std::chrono::duration<double, std::micro>(t3 - t2).count();

    std::cout << "Original stack_sweep_nd (with heap coords & sub_boxes): " << us1 << " us" << std::endl;
    std::cout << "Pure Stack stack_sweep_nd (zero heap allocations):     " << us2 << " us" << std::endl;
    std::cout << "Speedup: " << (us1 / us2) << "x" << std::endl;
    std::cout << "Match: " << (v1 == v2 ? "YES" : "FAIL") << std::endl;
    return 0;
}
