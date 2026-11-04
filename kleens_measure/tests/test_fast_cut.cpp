#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cassert>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"

using namespace kmp;

int64_t select_cut_slow(int axis, const Box& cell, const std::vector<Box>& boxes) {
    int64_t c_low = cell.low[axis], c_high = cell.high[axis];
    int64_t c_mid = c_low + (c_high - c_low) / 2;
    double cell_w = (double)(c_high - c_low);

    std::vector<int64_t> coords;
    coords.reserve(boxes.size() * 2);
    for (const auto& b : boxes) {
        if (b.low[axis] > c_low && b.low[axis] < c_high) coords.push_back(b.low[axis]);
        if (b.high[axis] > c_low && b.high[axis] < c_high) coords.push_back(b.high[axis]);
    }
    if (coords.empty()) return c_mid;
    std::sort(coords.begin(), coords.end());
    coords.erase(std::unique(coords.begin(), coords.end()), coords.end());

    size_t start_idx = (size_t)std::floor(0.20 * coords.size());
    size_t end_idx = (size_t)std::ceil(0.80 * coords.size());
    if (start_idx >= coords.size()) start_idx = 0;
    if (end_idx > coords.size() || end_idx <= start_idx) end_idx = coords.size();

    int64_t best_c = coords[coords.size() / 2];
    double min_cost = 1e18;
    for (size_t i = start_idx; i < end_idx; ++i) {
        int64_t cand = coords[i];
        int split_count = 0;
        for (const auto& b : boxes) {
            if (b.low[axis] < cand && b.high[axis] > cand) split_count++;
        }
        double dist_norm = (double)std::abs(cand - c_mid) / cell_w;
        double cost = (double)split_count + 2.0 * (dist_norm * dist_norm);
        if (cost < min_cost) {
            min_cost = cost;
            best_c = cand;
        }
    }
    return best_c;
}

int64_t select_cut_fast(int axis, const Box& cell, const std::vector<Box>& boxes) {
    int64_t c_low = cell.low[axis], c_high = cell.high[axis];
    int64_t c_mid = c_low + (c_high - c_low) / 2;
    double cell_w = (double)(c_high - c_low);

    std::vector<int64_t> coords;
    std::vector<int64_t> lows;
    std::vector<int64_t> highs;
    coords.reserve(boxes.size() * 2);
    lows.reserve(boxes.size());
    highs.reserve(boxes.size());

    for (const auto& b : boxes) {
        int64_t l = b.low[axis];
        int64_t h = b.high[axis];
        lows.push_back(l);
        highs.push_back(h);
        if (l > c_low && l < c_high) coords.push_back(l);
        if (h > c_low && h < c_high) coords.push_back(h);
    }
    if (coords.empty()) return c_mid;

    std::sort(coords.begin(), coords.end());
    coords.erase(std::unique(coords.begin(), coords.end()), coords.end());
    std::sort(lows.begin(), lows.end());
    std::sort(highs.begin(), highs.end());

    size_t start_idx = (size_t)std::floor(0.20 * coords.size());
    size_t end_idx = (size_t)std::ceil(0.80 * coords.size());
    if (start_idx >= coords.size()) start_idx = 0;
    if (end_idx > coords.size() || end_idx <= start_idx) end_idx = coords.size();

    int64_t best_c = coords[coords.size() / 2];
    double min_cost = 1e18;

    size_t p_low = 0, p_high = 0;
    size_t n_boxes = boxes.size();

    for (size_t i = 0; i < end_idx; ++i) {
        int64_t cand = coords[i];
        while (p_low < n_boxes && lows[p_low] < cand) p_low++;
        while (p_high < n_boxes && highs[p_high] <= cand) p_high++;

        if (i >= start_idx) {
            int split_count = (int)(p_low - p_high);
            double dist_norm = (double)std::abs(cand - c_mid) / cell_w;
            double cost = (double)split_count + 2.0 * (dist_norm * dist_norm);
            if (cost < min_cost) {
                min_cost = cost;
                best_c = cand;
            }
        }
    }
    return best_c;
}

int main() {
    auto boxes = generate_trellis_weave(1000, 4, 42);
    Box cell;
    cell.low.assign(4, 0);
    cell.high.assign(4, 1000000);

    auto t0 = std::chrono::high_resolution_clock::now();
    int64_t c_slow = select_cut_slow(0, cell, boxes);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_slow = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto t2 = std::chrono::high_resolution_clock::now();
    int64_t c_fast = select_cut_fast(0, cell, boxes);
    auto t3 = std::chrono::high_resolution_clock::now();
    double ms_fast = std::chrono::duration<double, std::milli>(t3 - t2).count();

    std::cout << "c_slow = " << c_slow << " (" << ms_slow << " ms)\n";
    std::cout << "c_fast = " << c_fast << " (" << ms_fast << " ms)\n";
    std::cout << "Speedup = " << (ms_slow / ms_fast) << "x\n";
    assert(c_slow == c_fast);
    std::cout << "Exact coordinate match verified!\n";
    return 0;
}
