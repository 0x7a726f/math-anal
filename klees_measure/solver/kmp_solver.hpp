#pragma once
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"
#include "split_policy.hpp"
#include <vector>
#include <algorithm>
#include <cassert>

namespace kmp {

class KMPSolver {
public:
    template<typename Policy = SplitPolicy>
    static Int128 solve(
        const std::vector<Box>& boxes,
        StructuralCounters& counters,
        const Policy& policy
    ) {
        counters.reset();
        std::vector<Box> valid_boxes;
        for (const auto& b : boxes) {
            if (b.is_valid()) valid_boxes.push_back(b);
        }
        if (valid_boxes.empty()) return Int128(0);

        int d = valid_boxes[0].dim();
        // Compute minimal bounding box of all inputs
        Box root_cell;
        root_cell.low.resize(d);
        root_cell.high.resize(d);
        for (int i = 0; i < d; ++i) {
            root_cell.low[i] = valid_boxes[0].low[i];
            root_cell.high[i] = valid_boxes[0].high[i];
        }
        for (const auto& b : valid_boxes) {
            for (int i = 0; i < d; ++i) {
                root_cell.low[i] = std::min(root_cell.low[i], b.low[i]);
                root_cell.high[i] = std::max(root_cell.high[i], b.high[i]);
            }
        }

        Int128 vol = solve_recursive(root_cell, valid_boxes, -1, counters, policy);
        counters.exact_volume = vol;
        return vol;
    }

private:
    template<typename Policy = SplitPolicy>
    static Int128 solve_recursive(
        const Box& cell,
        const std::vector<Box>& boxes,
        int last_split_dim,
        StructuralCounters& counters,
        const Policy& policy
    ) {
        counters.recursive_calls++;
        int d = cell.dim();

        // 1. Simplify: Clip boxes to cell, detect 0-slabs and 1-slabs
        std::vector<Box> active_boxes;
        active_boxes.reserve(boxes.size());

        // Track 1-slab intervals per dimension to check complete coverage
        std::vector<std::pair<int, std::pair<int64_t, int64_t>>> one_slabs;
        int one_slab_count_per_dim[6] = {0};

        for (const auto& b : boxes) {
            Box clipped = cell.intersect(b);
            if (!clipped.is_valid()) continue;

            // Check if 0-slab (covers cell completely)
            bool covers_cell = true;
            int non_cov_dim = -1;
            int non_cov_count = 0;

            for (int i = 0; i < d; ++i) {
                bool span_i = (clipped.low[i] <= cell.low[i] && clipped.high[i] >= cell.high[i]);
                if (!span_i) {
                    covers_cell = false;
                    non_cov_count++;
                    non_cov_dim = i;
                }
            }

            if (covers_cell) {
                // Cell is completely covered by a 0-slab
                return cell.volume();
            }

            if (non_cov_count == 1) {
                // 1-slab in non_cov_dim
                one_slabs.push_back({non_cov_dim, {clipped.low[non_cov_dim], clipped.high[non_cov_dim]}});
                one_slab_count_per_dim[non_cov_dim]++;
            }

            active_boxes.push_back(clipped);
        }

        if (active_boxes.empty()) {
            return Int128(0);
        }

        // Check if any dimension's 1-slabs completely cover C[i]
        for (int i = 0; i < d; ++i) {
            if (one_slab_count_per_dim[i] > 0) {
                std::vector<std::pair<int64_t, int64_t>> intervals;
                intervals.reserve(one_slab_count_per_dim[i]);
                for (const auto& item : one_slabs) {
                    if (item.first == i) intervals.push_back(item.second);
                }
                Int128 cov_len = exact_1d_union(intervals);
                if (cov_len >= Int128(cell.high[i] - cell.low[i])) {
                    // 1-slabs completely tile the cell along dimension i
                    return cell.volume();
                }
            }
        }

        // 2. Compute active (d-2)-face counts
        uint32_t d2_mat[6][6] = {0};
        uint64_t total_active_d2_faces = 0;

        for (const auto& b : active_boxes) {
            int non_cov[6];
            int n_nc = 0;
            for (int i = 0; i < d; ++i) {
                if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) {
                    non_cov[n_nc++] = i;
                }
            }
            if (n_nc == 2) {
                int u = non_cov[0];
                int v = non_cov[1];
                d2_mat[u][v]++;
                d2_mat[v][u]++;
                total_active_d2_faces++;
            } else if (n_nc > 2) {
                for (int p = 0; p < n_nc; ++p) {
                    for (int q = p + 1; q < n_nc; ++q) {
                        int u = non_cov[p];
                        int v = non_cov[q];
                        d2_mat[u][v]++;
                        d2_mat[v][u]++;
                        total_active_d2_faces++;
                    }
                }
            }
        }

        counters.total_d2_faces_processed += total_active_d2_faces;

        // 3. Base-Case / Trellis Cutoff check
        if (policy.should_terminate_leaf(cell, active_boxes, total_active_d2_faces)) {
            counters.trellis_leaves++;
            return policy.evaluate_leaf(cell, active_boxes, d);
        }

        // 4. Divide: Query split policy for axis and cut coordinate
        std::vector<std::vector<uint32_t>> d2_face_matrix(d, std::vector<uint32_t>(d, 0));
        for (int i = 0; i < d; ++i) {
            for (int j = 0; j < d; ++j) {
                d2_face_matrix[i][j] = d2_mat[i][j];
            }
        }

        // 4a. Check for Joint Orthogonal Corner 2-Cut
        if (policy.should_use_corner_2cut(cell, active_boxes, total_active_d2_faces)) {
            Corner2Cut c2 = policy.select_corner_2cut(cell, active_boxes, d2_face_matrix);
            if (c2.is_valid) {
                int ax1 = c2.axis1;
                int ax2 = c2.axis2;
                int64_t cut1 = c2.cut1;
                int64_t cut2 = c2.cut2;

                Box cell00 = cell, cell01 = cell, cell10 = cell, cell11 = cell;
                cell00.high[ax1] = cut1; cell00.high[ax2] = cut2;
                cell01.high[ax1] = cut1; cell01.low[ax2]  = cut2;
                cell10.low[ax1]  = cut1; cell10.high[ax2] = cut2;
                cell11.low[ax1]  = cut1; cell11.low[ax2]  = cut2;

                std::vector<Box> b00, b01, b10, b11;
                for (const auto& b : active_boxes) {
                    bool in_low1 = (b.low[ax1] < cut1);
                    bool in_high1 = (b.high[ax1] > cut1);
                    bool in_low2 = (b.low[ax2] < cut2);
                    bool in_high2 = (b.high[ax2] > cut2);

                    if (in_low1 && in_low2) b00.push_back(b);
                    if (in_low1 && in_high2) b01.push_back(b);
                    if (in_high1 && in_low2) b10.push_back(b);
                    if (in_high1 && in_high2) b11.push_back(b);
                }

                Int128 v00 = solve_recursive(cell00, b00, ax1, counters, policy);
                Int128 v01 = solve_recursive(cell01, b01, ax2, counters, policy);
                Int128 v10 = solve_recursive(cell10, b10, ax1, counters, policy);
                Int128 v11 = solve_recursive(cell11, b11, ax2, counters, policy);

                return v00 + v01 + v10 + v11;
            }
        }

        int axis = policy.select_split_axis(cell, active_boxes, d2_face_matrix, last_split_dim);
        int64_t cut = policy.select_cut_coordinate(axis, cell, active_boxes);

        // Guard against degenerate cuts
        if (cut <= cell.low[axis] || cut >= cell.high[axis]) {
            cut = cell.low[axis] + (cell.high[axis] - cell.low[axis]) / 2;
        }

        if (cut <= cell.low[axis] || cut >= cell.high[axis]) {
            // Cannot split further along this axis
            counters.trellis_leaves++;
            return exact_kmp_sweep(active_boxes, d);
        }

        // Subcell 1: [cell.low, cut]
        Box cell1 = cell;
        cell1.high[axis] = cut;

        // Subcell 2: [cut, cell.high]
        Box cell2 = cell;
        cell2.low[axis] = cut;

        std::vector<Box> boxes1;
        std::vector<Box> boxes2;
        boxes1.reserve(active_boxes.size());
        boxes2.reserve(active_boxes.size());

        for (const auto& b : active_boxes) {
            if (b.low[axis] < cut) boxes1.push_back(b);
            if (b.high[axis] > cut) boxes2.push_back(b);
        }

        Int128 vol1 = solve_recursive(cell1, boxes1, axis, counters, policy);
        Int128 vol2 = solve_recursive(cell2, boxes2, axis, counters, policy);

        return vol1 + vol2;
    }
};

} // namespace kmp
