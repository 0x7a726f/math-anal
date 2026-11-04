#include <iostream>
#include <chrono>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

// Optimized 2D measure
inline Int128 fast_exact_2d(const std::vector<Box>& boxes) {
    if (boxes.empty()) return Int128(0);
    if (boxes.size() == 1) return boxes[0].volume();

    // Collect x-coordinates
    std::vector<int64_t> x_coords;
    x_coords.reserve(boxes.size() * 2);
    for (const auto& b : boxes) {
        x_coords.push_back(b.low[0]);
        x_coords.push_back(b.high[0]);
    }
    std::sort(x_coords.begin(), x_coords.end());
    x_coords.erase(std::unique(x_coords.begin(), x_coords.end()), x_coords.end());

    Int128 total_vol(0);
    std::vector<std::pair<int64_t, int64_t>> intervals;
    intervals.reserve(boxes.size());

    for (size_t k = 0; k + 1 < x_coords.size(); ++k) {
        int64_t x_l = x_coords[k];
        int64_t x_r = x_coords[k + 1];
        if (x_r <= x_l) continue;

        intervals.clear();
        for (const auto& b : boxes) {
            if (b.low[0] <= x_l && b.high[0] >= x_r) {
                intervals.push_back({b.low[1], b.high[1]});
            }
        }
        if (!intervals.empty()) {
            Int128 y_len = exact_1d_union(intervals);
            total_vol += y_len * (x_r - x_l);
        }
    }
    return total_vol;
}

// Fast recursive sweep with single-box shortcut & fast 2D
inline Int128 fast_kmp_sweep(const std::vector<Box>& boxes, int d) {
    if (boxes.empty() || d <= 0) return Int128(0);
    if (boxes.size() == 1) return boxes[0].volume();
    if (d == 1) {
        std::vector<std::pair<int64_t, int64_t>> intervals;
        intervals.reserve(boxes.size());
        for (const auto& b : boxes) intervals.push_back({b.low[0], b.high[0]});
        return exact_1d_union(intervals);
    }
    if (d == 2) {
        return fast_exact_2d(boxes);
    }

    int last_dim = d - 1;
    std::vector<int64_t> coords;
    coords.reserve(boxes.size() * 2);
    for (const auto& b : boxes) {
        coords.push_back(b.low[last_dim]);
        coords.push_back(b.high[last_dim]);
    }
    std::sort(coords.begin(), coords.end());
    coords.erase(std::unique(coords.begin(), coords.end()), coords.end());

    Int128 total_volume(0);
    std::vector<Box> sub_boxes;
    sub_boxes.reserve(boxes.size());

    for (size_t k = 0; k + 1 < coords.size(); ++k) {
        int64_t slab_l = coords[k];
        int64_t slab_r = coords[k + 1];
        if (slab_r <= slab_l) continue;

        sub_boxes.clear();
        for (const auto& b : boxes) {
            if (b.low[last_dim] <= slab_l && b.high[last_dim] >= slab_r) {
                Box proj;
                proj.low.assign(b.low.begin(), b.low.begin() + last_dim);
                proj.high.assign(b.high.begin(), b.high.begin() + last_dim);
                sub_boxes.push_back(std::move(proj));
            }
        }

        if (!sub_boxes.empty()) {
            Int128 cross_vol = fast_kmp_sweep(sub_boxes, d - 1);
            total_volume += cross_vol * (slab_r - slab_l);
        }
    }
    return total_volume;
}

int main() {
    for (int d : {3, 4, 5, 6}) {
        for (int n : {10, 20, 35, 64}) {
            auto boxes = generate_trellis_weave(n, d, 42);

            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 v1 = exact_kmp_sweep(boxes, d);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms1 = std::chrono::duration<double, std::milli>(t1 - t0).count();

            auto t2 = std::chrono::high_resolution_clock::now();
            Int128 v2 = fast_kmp_sweep(boxes, d);
            auto t3 = std::chrono::high_resolution_clock::now();
            double ms2 = std::chrono::duration<double, std::milli>(t3 - t2).count();

            if (v1 != v2) {
                std::cerr << "Mismatch!" << std::endl;
                return 1;
            }
            std::cout << "d=" << d << " n=" << n << " | orig: " << ms1 << " ms | fast: " << ms2 << " ms | speedup: " << (ms1/ms2) << "x\n";
            if (ms1 > 1000.0) break;
        }
    }
    return 0;
}
