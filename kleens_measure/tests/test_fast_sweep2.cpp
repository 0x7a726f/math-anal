#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"

using namespace kmp;

struct StackBox {
    int64_t low[6];
    int64_t high[6];
    int d;

    Int128 volume() const {
        Int128 v(1);
        for (int i = 0; i < d; ++i) {
            if (high[i] <= low[i]) return Int128(0);
            v = v * (high[i] - low[i]);
        }
        return v;
    }
};

inline Int128 stack_1d_union(std::pair<int64_t, int64_t>* intervals, size_t count) {
    if (count == 0) return Int128(0);
    std::sort(intervals, intervals + count);
    Int128 total(0);
    int64_t cur_l = intervals[0].first;
    int64_t cur_r = intervals[0].second;
    for (size_t i = 1; i < count; ++i) {
        if (intervals[i].first <= cur_r) {
            cur_r = std::max(cur_r, intervals[i].second);
        } else {
            total += Int128(cur_r - cur_l);
            cur_l = intervals[i].first;
            cur_r = intervals[i].second;
        }
    }
    total += Int128(cur_r - cur_l);
    return total;
}

inline Int128 stack_sweep_2d(const StackBox* boxes, size_t n) {
    if (n == 0) return Int128(0);
    if (n == 1) return boxes[0].volume();

    int64_t x_coords[512];
    size_t x_cnt = 0;
    for (size_t i = 0; i < n; ++i) {
        x_coords[x_cnt++] = boxes[i].low[0];
        x_coords[x_cnt++] = boxes[i].high[0];
    }
    std::sort(x_coords, x_coords + x_cnt);
    x_cnt = std::unique(x_coords, x_coords + x_cnt) - x_coords;

    Int128 total_vol(0);
    std::pair<int64_t, int64_t> intervals[256];

    for (size_t k = 0; k + 1 < x_cnt; ++k) {
        int64_t x_l = x_coords[k];
        int64_t x_r = x_coords[k + 1];
        if (x_r <= x_l) continue;

        size_t int_cnt = 0;
        for (size_t i = 0; i < n; ++i) {
            if (boxes[i].low[0] <= x_l && boxes[i].high[0] >= x_r) {
                intervals[int_cnt++] = {boxes[i].low[1], boxes[i].high[1]};
            }
        }
        if (int_cnt > 0) {
            Int128 y_len = stack_1d_union(intervals, int_cnt);
            total_vol += y_len * (x_r - x_l);
        }
    }
    return total_vol;
}

Int128 stack_sweep(const StackBox* boxes, size_t n, int d) {
    if (n == 0 || d <= 0) return Int128(0);
    if (n == 1) return boxes[0].volume();

    if (d == 1) {
        std::pair<int64_t, int64_t> intervals[256];
        for (size_t i = 0; i < n; ++i) intervals[i] = {boxes[i].low[0], boxes[i].high[0]};
        return stack_1d_union(intervals, n);
    }
    if (d == 2) {
        return stack_sweep_2d(boxes, n);
    }

    int last_dim = d - 1;
    int64_t coords[512];
    size_t c_cnt = 0;
    for (size_t i = 0; i < n; ++i) {
        coords[c_cnt++] = boxes[i].low[last_dim];
        coords[c_cnt++] = boxes[i].high[last_dim];
    }
    std::sort(coords, coords + c_cnt);
    c_cnt = std::unique(coords, coords + c_cnt) - coords;

    Int128 total_volume(0);
    StackBox sub_boxes[256];

    for (size_t k = 0; k + 1 < c_cnt; ++k) {
        int64_t slab_l = coords[k];
        int64_t slab_r = coords[k + 1];
        if (slab_r <= slab_l) continue;

        size_t sub_cnt = 0;
        for (size_t i = 0; i < n; ++i) {
            if (boxes[i].low[last_dim] <= slab_l && boxes[i].high[last_dim] >= slab_r) {
                StackBox& sb = sub_boxes[sub_cnt++];
                sb.d = last_dim;
                for (int j = 0; j < last_dim; ++j) {
                    sb.low[j] = boxes[i].low[j];
                    sb.high[j] = boxes[i].high[j];
                }
            }
        }

        if (sub_cnt > 0) {
            Int128 cross_vol = stack_sweep(sub_boxes, sub_cnt, last_dim);
            total_volume += cross_vol * (slab_r - slab_l);
        }
    }
    return total_volume;
}

Int128 stack_sweep_wrapper(const std::vector<Box>& boxes, int d) {
    std::vector<StackBox> sboxes;
    sboxes.reserve(boxes.size());
    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        StackBox sb;
        sb.d = d;
        for (int i = 0; i < d; ++i) {
            sb.low[i] = b.low[i];
            sb.high[i] = b.high[i];
        }
        sboxes.push_back(sb);
    }
    return stack_sweep(sboxes.data(), sboxes.size(), d);
}

int main() {
    std::cout << "Testing stack_sweep timing and accuracy..." << std::endl;
    for (int d = 3; d <= 6; ++d) {
        for (int n : {10, 20, 30, 40, 50, 64}) {
            auto boxes = generate_trellis_weave(n, d, 42);
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 vol = stack_sweep_wrapper(boxes, d);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "d=" << d << " n=" << std::setw(3) << n << " | ms=" << std::setw(8) << ms << std::endl;
            if (ms > 1000.0) break;
        }
    }
    return 0;
}
