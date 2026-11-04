#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

struct FastSolverPolicy {
    std::string id = "FastChampion";
    int thresh_d3 = 64;
    int thresh_d4 = 48;
    int thresh_d5 = 24;
    int thresh_d6 = 22;

    int select_split_axis(const SweepStackBox& cell, const SweepStackBox* boxes, size_t n, int last_dim) const {
        int d = cell.d;
        int best_dim = -1;
        double max_eff = -1.0;

        int span_count[6] = {0};
        int trans_span[6] = {0};

        for (size_t k = 0; k < n; ++k) {
            int mask = 0;
            for (int i = 0; i < d; ++i) {
                if (boxes[k].low[i] <= cell.low[i] && boxes[k].high[i] >= cell.high[i]) {
                    mask |= (1 << i);
                }
            }
            int noncov = d - __builtin_popcount(mask);
            for (int i = 0; i < d; ++i) {
                if (mask & (1 << i)) {
                    span_count[i]++;
                    if (noncov >= 2) trans_span[i]++;
                }
            }
        }

        // Active ridge degrees
        uint32_t d2_mat[6][6] = {0};
        for (size_t k = 0; k < n; ++k) {
            int nc[6];
            int n_nc = 0;
            for (int i = 0; i < d; ++i) {
                if (boxes[k].low[i] > cell.low[i] || boxes[k].high[i] < cell.high[i]) {
                    nc[n_nc++] = i;
                }
            }
            if (n_nc == 2) {
                d2_mat[nc[0]][nc[1]]++;
                d2_mat[nc[1]][nc[0]]++;
            } else if (n_nc > 2) {
                for (int p = 0; p < n_nc; ++p) {
                    for (int q = p + 1; q < n_nc; ++q) {
                        d2_mat[nc[p]][nc[q]]++;
                        d2_mat[nc[q]][nc[p]]++;
                    }
                }
            }
        }

        for (int i = 0; i < d; ++i) {
            double w_deg = 0.0;
            for (int j = 0; j < d; ++j) {
                if (i != j && d2_mat[i][j] > 0) {
                    double n_ij = (double)d2_mat[i][j];
                    w_deg += n_ij * std::sqrt(n_ij);
                }
            }
            if (w_deg <= 0.0) continue;
            double denom = (1.0 + (double)span_count[i]);
            double trans_pen = (1.0 + 0.15 * (double)trans_span[i]);
            double eff = w_deg / (denom * denom * trans_pen);
            if (eff > max_eff) {
                max_eff = eff;
                best_dim = i;
            }
        }
        if (best_dim != -1 && max_eff > 0.0) return best_dim;
        return (last_dim + 1) % d;
    }

    int64_t select_cut(int axis, const SweepStackBox& cell, const SweepStackBox* boxes, size_t n) const {
        int64_t c_low = cell.low[axis], c_high = cell.high[axis];
        int64_t c_mid = c_low + (c_high - c_low) / 2;
        double cell_w = (double)(c_high - c_low);

        std::vector<int64_t> coords;
        std::vector<int64_t> lows, highs;
        coords.reserve(n * 2);
        lows.reserve(n);
        highs.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            int64_t l = boxes[i].low[axis], h = boxes[i].high[axis];
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

        for (size_t i = 0; i < end_idx; ++i) {
            int64_t cand = coords[i];
            while (p_low < n && lows[p_low] < cand) p_low++;
            while (p_high < n && highs[p_high] <= cand) p_high++;

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
};

Int128 solve_fast_rec(const SweepStackBox& cell, const std::vector<SweepStackBox>& boxes, int last_dim, StructuralCounters& c, const FastSolverPolicy& policy) {
    c.recursive_calls++;
    int d = cell.d;

    std::vector<SweepStackBox> active;
    active.reserve(boxes.size());

    for (const auto& b : boxes) {
        SweepStackBox clipped;
        clipped.d = d;
        bool valid = true;
        bool covers = true;
        for (int i = 0; i < d; ++i) {
            clipped.low[i] = std::max(cell.low[i], b.low[i]);
            clipped.high[i] = std::min(cell.high[i], b.high[i]);
            if (clipped.high[i] <= clipped.low[i]) { valid = false; break; }
            if (clipped.low[i] > cell.low[i] || clipped.high[i] < cell.high[i]) covers = false;
        }
        if (!valid) continue;
        if (covers) return cell.volume();
        active.push_back(clipped);
    }
    if (active.empty()) return Int128(0);

    int thresh = policy.thresh_d3;
    if (d >= 6) thresh = policy.thresh_d6;
    else if (d == 5) thresh = policy.thresh_d5;
    else if (d == 4) thresh = policy.thresh_d4;

    if ((int)active.size() <= thresh) {
        c.trellis_leaves++;
        if (active.size() == 1) return active[0].volume();
        return stack_sweep_nd(active.data(), active.size(), d);
    }

    int axis = policy.select_split_axis(cell, active.data(), active.size(), last_dim);
    int64_t cut = policy.select_cut(axis, cell, active.data(), active.size());

    if (cut <= cell.low[axis] || cut >= cell.high[axis]) {
        cut = cell.low[axis] + (cell.high[axis] - cell.low[axis]) / 2;
    }
    if (cut <= cell.low[axis] || cut >= cell.high[axis]) {
        c.trellis_leaves++;
        return stack_sweep_nd(active.data(), active.size(), d);
    }

    SweepStackBox cell1 = cell; cell1.high[axis] = cut;
    SweepStackBox cell2 = cell; cell2.low[axis] = cut;

    std::vector<SweepStackBox> boxes1, boxes2;
    boxes1.reserve(active.size());
    boxes2.reserve(active.size());

    for (const auto& b : active) {
        if (b.low[axis] < cut) boxes1.push_back(b);
        if (b.high[axis] > cut) boxes2.push_back(b);
    }

    return solve_fast_rec(cell1, boxes1, axis, c, policy) + solve_fast_rec(cell2, boxes2, axis, c, policy);
}

int main() {
    FastSolverPolicy policy;
    std::cout << "Testing FastSolver with SweepStackBox on d=6 n=256 and n=512..." << std::endl;
    for (int n : {256, 512}) {
        auto raw_boxes = generate_trellis_weave(n, 6, 42);
        SweepStackBox root;
        root.d = 6;
        for (int i = 0; i < 6; ++i) { root.low[i] = 0; root.high[i] = 1000000; }
        std::vector<SweepStackBox> sboxes(n);
        for (int k = 0; k < n; ++k) {
            sboxes[k].d = 6;
            for (int i = 0; i < 6; ++i) {
                sboxes[k].low[i] = raw_boxes[k].low[i];
                sboxes[k].high[i] = raw_boxes[k].high[i];
            }
        }
        StructuralCounters c;
        auto t0 = std::chrono::high_resolution_clock::now();
        Int128 vol = solve_fast_rec(root, sboxes, -1, c, policy);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "d=6 n=" << n << " | ms=" << ms << " calls=" << c.recursive_calls << " vol=" << int128_to_string(vol) << std::endl;
    }
    return 0;
}
