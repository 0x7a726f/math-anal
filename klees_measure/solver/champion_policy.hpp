#pragma once
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"
#include "clean_cell_trace.hpp"
#include "tensor_clean_cell.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace kmp {

struct ChampionPolicy {
    std::string id = "Champion H-51/H-86: Active Ridge Minimization + AVX2 Tensor Clean-Cell";

    int thresh_d3 = 64;
    int thresh_d4 = 48;
    int thresh_d5 = 24;
    int thresh_d6 = 18;

    int select_split_axis(
        const Box& cell,
        const std::vector<Box>& boxes,
        const std::vector<std::vector<uint32_t>>& d2_face_matrix,
        int last_split_dim
    ) const {
        int d = cell.dim();
        int best_dim = -1;
        double max_efficiency = -1.0;

        int spanning_count[6] = {0};
        int transverse_spanning[6] = {0};

        for (const auto& b : boxes) {
            int span_mask = 0;
            for (int i = 0; i < d; ++i) {
                if (b.low[i] <= cell.low[i] && b.high[i] >= cell.high[i]) {
                    span_mask |= (1 << i);
                }
            }
            int noncov_total = d - __builtin_popcount(span_mask);
            for (int i = 0; i < d; ++i) {
                if (span_mask & (1 << i)) {
                    spanning_count[i]++;
                    if (noncov_total >= 2) transverse_spanning[i]++;
                }
            }
        }

        for (int i = 0; i < d; ++i) {
            double weighted_deg = 0.0;
            for (int j = 0; j < d; ++j) {
                if (i != j && d2_face_matrix[i][j] > 0) {
                    double n_ij = (double)d2_face_matrix[i][j];
                    weighted_deg += n_ij * std::sqrt(n_ij); // n^{1.5}
                }
            }
            if (weighted_deg <= 0.0) continue;

            double denom = (1.0 + (double)spanning_count[i]);
            double trans_penalty = (1.0 + 0.15 * (double)transverse_spanning[i]);
            double efficiency = weighted_deg / (denom * denom * trans_penalty);
            if (efficiency > max_efficiency) {
                max_efficiency = efficiency;
                best_dim = i;
            }
        }

        if (best_dim != -1 && max_efficiency > 0.0) {
            return best_dim;
        }

        return (last_split_dim + 1) % d;
    }

    int64_t select_cut_coordinate(
        int axis,
        const Box& cell,
        const std::vector<Box>& boxes
    ) const {
        int64_t c_low = cell.low[axis];
        int64_t c_high = cell.high[axis];
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

        if (coords.empty()) {
            return c_mid;
        }

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

    bool use_corner_2cuts = true;
    uint64_t corner_2cut_face_threshold = 3000;

    Corner2Cut select_corner_2cut(
        const Box& cell,
        const std::vector<Box>& boxes,
        const std::vector<std::vector<uint32_t>>& d2_face_matrix
    ) const {
        int d = cell.dim();
        if (d < 2) return {-1, -1, 0, 0, false};

        int best_i = -1, best_j = -1;
        uint32_t max_faces = 0;
        for (int i = 0; i < d; ++i) {
            for (int j = i + 1; j < d; ++j) {
                if (d2_face_matrix[i][j] > max_faces) {
                    max_faces = d2_face_matrix[i][j];
                    best_i = i;
                    best_j = j;
                }
            }
        }

        if (best_i == -1) {
            best_i = 0;
            best_j = 1;
        }

        int64_t cut_i = select_cut_coordinate(best_i, cell, boxes);
        int64_t cut_j = select_cut_coordinate(best_j, cell, boxes);

        if (cut_i <= cell.low[best_i] || cut_i >= cell.high[best_i] ||
            cut_j <= cell.low[best_j] || cut_j >= cell.high[best_j]) {
            return {-1, -1, 0, 0, false};
        }

        return {best_i, best_j, cut_i, cut_j, true};
    }

    bool should_use_corner_2cut(
        const Box& cell,
        const std::vector<Box>& boxes,
        uint64_t active_d2_faces
    ) const {
        if (!use_corner_2cuts) return false;
        return active_d2_faces >= corner_2cut_face_threshold;
    }

    bool should_terminate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        uint64_t active_d2_faces
    ) const {
        if (boxes.empty()) return true;
        int d = cell.dim();
        int threshold = thresh_d3;
        if (d >= 6) threshold = thresh_d6;
        else if (d == 5) threshold = thresh_d5;
        else if (d == 4) threshold = thresh_d4;
        if ((int)boxes.size() <= threshold) return true;
        if (active_d2_faces == 0) return true;
        if (is_clean_cell(cell, boxes)) return true;
        if ((d == 4 || d == 5 || d == 6) && is_hypergraph_clean_cell(cell, boxes)) return true;
        return false;
    }

    Int128 evaluate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        int d
    ) const {
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();
        if (is_clean_cell(cell, boxes)) {
            return evaluate_tensor_clean_cell(cell, boxes);
        }
        if (d == 4 && is_hypergraph_clean_cell(cell, boxes)) {
            return evaluate_tensor_hypergraph_d4(cell, boxes);
        }
        if (d == 5 && is_hypergraph_clean_cell(cell, boxes)) {
            return evaluate_tensor_hypergraph_d5(cell, boxes);
        }
        if (d == 6 && is_hypergraph_clean_cell(cell, boxes)) {
            return evaluate_tensor_hypergraph_d6(cell, boxes);
        }
        return exact_kmp_sweep(boxes, d);
    }
};

} // namespace kmp
