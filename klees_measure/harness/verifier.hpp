#pragma once
#include "types.hpp"
#include <vector>
#include <algorithm>
#include <cassert>

namespace kmp {

// Exact 1D Interval Union with Int128 accumulation
inline Int128 exact_1d_union(std::vector<std::pair<int64_t, int64_t>>& intervals) {
    if (intervals.empty()) return Int128(0);
    std::sort(intervals.begin(), intervals.end());
    Int128 total(0);
    int64_t cur_l = intervals[0].first;
    int64_t cur_r = intervals[0].second;

    for (size_t i = 1; i < intervals.size(); ++i) {
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

// Exact Recursive Hyperplane Sweep in R^d with exact Int128 arithmetic
struct SweepStackBox {
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

inline Int128 stack_sweep_1d(std::pair<int64_t, int64_t>* intervals, size_t count) {
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

inline Int128 stack_sweep_2d(const SweepStackBox* boxes, size_t n) {
    if (n == 0) return Int128(0);
    if (n == 1) return boxes[0].volume();

    std::vector<int64_t> x_coords;
    x_coords.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        x_coords.push_back(boxes[i].low[0]);
        x_coords.push_back(boxes[i].high[0]);
    }
    std::sort(x_coords.begin(), x_coords.end());
    x_coords.erase(std::unique(x_coords.begin(), x_coords.end()), x_coords.end());

    Int128 total_vol(0);
    std::pair<int64_t, int64_t> stack_intervals[128];
    std::vector<std::pair<int64_t, int64_t>> heap_intervals;
    std::pair<int64_t, int64_t>* intervals_ptr = stack_intervals;
    if (n > 128) {
        heap_intervals.resize(n);
        intervals_ptr = heap_intervals.data();
    }

    for (size_t k = 0; k + 1 < x_coords.size(); ++k) {
        int64_t x_l = x_coords[k];
        int64_t x_r = x_coords[k + 1];
        if (x_r <= x_l) continue;

        size_t count = 0;
        for (size_t i = 0; i < n; ++i) {
            if (boxes[i].low[0] <= x_l && boxes[i].high[0] >= x_r) {
                intervals_ptr[count++] = {boxes[i].low[1], boxes[i].high[1]};
            }
        }
        if (count > 0) {
            Int128 y_len = stack_sweep_1d(intervals_ptr, count);
            total_vol += y_len * (x_r - x_l);
        }
    }
    return total_vol;
}

inline Int128 stack_sweep_nd(const SweepStackBox* boxes, size_t n, int d) {
    if (n == 0 || d <= 0) return Int128(0);
    if (n == 1) return boxes[0].volume();

    if (d == 1) {
        std::pair<int64_t, int64_t> stack_buf[128];
        std::vector<std::pair<int64_t, int64_t>> heap_buf;
        std::pair<int64_t, int64_t>* ptr = stack_buf;
        if (n > 128) {
            heap_buf.resize(n);
            ptr = heap_buf.data();
        }
        for (size_t i = 0; i < n; ++i) ptr[i] = {boxes[i].low[0], boxes[i].high[0]};
        return stack_sweep_1d(ptr, n);
    }
    if (d == 2) {
        return stack_sweep_2d(boxes, n);
    }

    int last_dim = d - 1;
    if (n <= 48) {
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
                Int128 cross_vol = stack_sweep_nd(stack_sub_boxes, sub_count, last_dim);
                total_volume += cross_vol * (slab_r - slab_l);
            }
        }
        return total_volume;
    }

    std::vector<int64_t> coords;
    coords.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        coords.push_back(boxes[i].low[last_dim]);
        coords.push_back(boxes[i].high[last_dim]);
    }
    std::sort(coords.begin(), coords.end());
    coords.erase(std::unique(coords.begin(), coords.end()), coords.end());

    Int128 total_volume(0);
    std::vector<SweepStackBox> sub_boxes;
    sub_boxes.reserve(n);

    for (size_t k = 0; k + 1 < coords.size(); ++k) {
        int64_t slab_l = coords[k];
        int64_t slab_r = coords[k + 1];
        if (slab_r <= slab_l) continue;

        sub_boxes.clear();
        for (size_t i = 0; i < n; ++i) {
            if (boxes[i].low[last_dim] <= slab_l && boxes[i].high[last_dim] >= slab_r) {
                SweepStackBox sb;
                sb.d = last_dim;
                for (int j = 0; j < last_dim; ++j) {
                    sb.low[j] = boxes[i].low[j];
                    sb.high[j] = boxes[i].high[j];
                }
                sub_boxes.push_back(sb);
            }
        }

        if (!sub_boxes.empty()) {
            Int128 cross_vol = stack_sweep_nd(sub_boxes.data(), sub_boxes.size(), last_dim);
            total_volume += cross_vol * (slab_r - slab_l);
        }
    }
    return total_volume;
}

inline Int128 exact_kmp_sweep(const std::vector<Box>& boxes, int d, StructuralCounters* counters = nullptr) {
    if (counters) {
        counters->total_tensor_arithmetic_ops += (uint64_t)boxes.size() * (uint64_t)std::max(1, d);
    }
    if (boxes.empty() || d <= 0) return Int128(0);
    if (d > 6) {
        // Fallback for d > 6
        std::vector<Box> valid;
        for (const auto& b : boxes) if (b.is_valid()) valid.push_back(b);
        if (valid.empty()) return Int128(0);
        if (valid.size() == 1) return valid[0].volume();
        int last_dim = d - 1;
        std::vector<int64_t> coords;
        for (const auto& b : valid) { coords.push_back(b.low[last_dim]); coords.push_back(b.high[last_dim]); }
        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());
        Int128 total_volume(0);
        for (size_t k = 0; k + 1 < coords.size(); ++k) {
            int64_t slab_l = coords[k], slab_r = coords[k + 1];
            if (slab_r <= slab_l) continue;
            std::vector<Box> sub_boxes;
            for (const auto& b : valid) {
                if (b.low[last_dim] <= slab_l && b.high[last_dim] >= slab_r) {
                    Box proj;
                    proj.low.assign(b.low.begin(), b.low.begin() + last_dim);
                    proj.high.assign(b.high.begin(), b.high.begin() + last_dim);
                    sub_boxes.push_back(proj);
                }
            }
            if (!sub_boxes.empty()) {
                total_volume += exact_kmp_sweep(sub_boxes, d - 1) * (slab_r - slab_l);
            }
        }
        return total_volume;
    }

    std::vector<SweepStackBox> sboxes;
    sboxes.reserve(boxes.size());
    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        SweepStackBox sb;
        sb.d = d;
        for (int i = 0; i < d; ++i) {
            sb.low[i] = b.low[i];
            sb.high[i] = b.high[i];
        }
        sboxes.push_back(sb);
    }
    return stack_sweep_nd(sboxes.data(), sboxes.size(), d);
}

// Top-level verifier
inline Int128 exact_verify_volume(const std::vector<Box>& boxes) {
    if (boxes.empty()) return Int128(0);
    int d = boxes[0].dim();
    return exact_kmp_sweep(boxes, d);
}

} // namespace kmp
