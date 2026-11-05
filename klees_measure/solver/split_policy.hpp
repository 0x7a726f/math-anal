#pragma once
#include "../harness/types.hpp"
#include "clean_cell_trace.hpp"
#include "tensor_clean_cell.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace kmp {

struct SplitPolicy {
    std::string id = "H-86: Clean-Cell 4D Tensor Hook (Immediate Leaf Evaluation)";

    int leaf_box_threshold = 500;
    uint64_t leaf_d2_face_threshold = 0;
    double cut_plane_ratio = 0.50;

    double get_face_weight(int i, int j, int d) const {
        return 1.0;
    }

    int select_split_axis(
        const Box& cell,
        const std::vector<Box>& boxes,
        const std::vector<std::vector<uint32_t>>& d2_face_matrix,
        int last_split_dim
    ) const {
        int d = cell.dim();
        int best_dim = -1;
        double max_efficiency = -1.0;

        for (int i = 0; i < d; ++i) {
            double weighted_deg = 0.0;
            for (int j = 0; j < d; ++j) {
                if (i != j && d2_face_matrix[i][j] > 0) {
                    double n_ij = (double)d2_face_matrix[i][j];
                    weighted_deg += std::pow(n_ij, 1.5);
                }
            }
            if (weighted_deg <= 0.0) continue;

            int spanning_count = 0;
            int transverse_spanning = 0;
            for (const auto& b : boxes) {
                if (b.low[i] <= cell.low[i] && b.high[i] >= cell.high[i]) {
                    spanning_count++;
                    int other_noncov = 0;
                    for (int j = 0; j < d; ++j) {
                        if (j != i && (b.low[j] > cell.low[j] || b.high[j] < cell.high[j])) {
                            other_noncov++;
                        }
                    }
                    if (other_noncov >= 2) transverse_spanning++;
                }
            }

            double denom = (1.0 + (double)spanning_count);
            double trans_penalty = (1.0 + 0.15 * (double)transverse_spanning);
            double art_factor = 1.0;
            double efficiency = (weighted_deg * art_factor) / (denom * denom * trans_penalty);
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
        coords.reserve(boxes.size() * 2);
        for (const auto& b : boxes) {
            if (b.low[axis] > c_low && b.low[axis] < c_high) coords.push_back(b.low[axis]);
            if (b.high[axis] > c_low && b.high[axis] < c_high) coords.push_back(b.high[axis]);
        }

        if (coords.empty()) {
            return c_mid;
        }

        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());

        size_t start_idx = (size_t)std::floor(0.2 * coords.size());
        size_t end_idx = (size_t)std::ceil(0.8 * coords.size());
        if (start_idx >= coords.size()) start_idx = 0;
        if (end_idx > coords.size() || end_idx <= start_idx) end_idx = coords.size();

        
        
        
        
        
        
        

        int64_t best_c = coords[coords.size() / 2];
        double min_cost = 1e18;

        for (size_t i = start_idx; i < end_idx; ++i) {
            int64_t cand = coords[i];
            int split_count = 0;
            for (const auto& b : boxes) {
                if (b.low[axis] < cand && b.high[axis] > cand) {
                    split_count++;
                }
            }
            double dist_norm = (double)std::abs(cand - c_mid) / cell_w;
            double cost = (double)split_count + 2.0 * std::pow(dist_norm, 2.0);
            if (cost < min_cost) {
                min_cost = cost;
                best_c = cand;
            }
        }

        return best_c;
    }

    bool should_terminate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        uint64_t active_d2_faces
    ) const {
        if (boxes.empty()) return true;
        int d = cell.dim();
        int threshold = leaf_box_threshold;
        if (d >= 6) threshold = std::min(threshold, 22);
        else if (d == 5) threshold = std::min(threshold, 24);
        else if (d == 4) threshold = std::min(threshold, 500);
        if ((int)boxes.size() <= threshold) return true;
        if (active_d2_faces <= leaf_d2_face_threshold) return true;
        if (d <= 4 && is_clean_cell(cell, boxes)) return true;
        if (d <= 6 && (int)boxes.size() <= 16 && is_clean_cell(cell, boxes)) return true;
        return false;
    }

    Int128 evaluate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        int d
    ) const {
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();

        if (d <= 6 && is_clean_cell(cell, boxes)) {
            return evaluate_tensor_clean_cell(cell, boxes);
        }
        return exact_kmp_sweep(boxes, d);
    }
};

inline SplitPolicy& get_split_policy() {
    static SplitPolicy policy;
    return policy;
}

} // namespace kmp
