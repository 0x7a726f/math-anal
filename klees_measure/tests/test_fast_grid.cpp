#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <string>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"

using namespace kmp;

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

inline Int128 fast_exact_2d(const std::vector<Box>& boxes) {
    if (boxes.empty()) return Int128(0);
    if (boxes.size() == 1) return boxes[0].volume();

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

inline Int128 fast_kmp_sweep(const std::vector<Box>& boxes, int d) {
    std::vector<Box> valid;
    valid.reserve(boxes.size());
    for (const auto& b : boxes) {
        if (b.is_valid()) valid.push_back(b);
    }
    if (valid.empty() || d <= 0) return Int128(0);
    if (valid.size() == 1) return valid[0].volume();

    if (d == 1) {
        std::vector<std::pair<int64_t, int64_t>> intervals;
        intervals.reserve(valid.size());
        for (const auto& b : valid) intervals.push_back({b.low[0], b.high[0]});
        return exact_1d_union(intervals);
    }
    if (d == 2) {
        return fast_exact_2d(valid);
    }

    int last_dim = d - 1;
    std::vector<int64_t> coords;
    coords.reserve(valid.size() * 2);
    for (const auto& b : valid) {
        coords.push_back(b.low[last_dim]);
        coords.push_back(b.high[last_dim]);
    }
    std::sort(coords.begin(), coords.end());
    coords.erase(std::unique(coords.begin(), coords.end()), coords.end());

    Int128 total_volume(0);
    std::vector<Box> sub_boxes;
    sub_boxes.reserve(valid.size());

    for (size_t k = 0; k + 1 < coords.size(); ++k) {
        int64_t slab_l = coords[k];
        int64_t slab_r = coords[k + 1];
        if (slab_r <= slab_l) continue;

        sub_boxes.clear();
        for (const auto& b : valid) {
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

#include "../solver/clean_cell_trace.hpp"

struct FastChampionPolicy {
    std::string id = "H-51 Champion: Centered GSAH + Transverse + Adaptive Cutoff";

    int get_threshold(int d) const {
        if (d >= 6) return 14;
        if (d == 5) return 24;
        if (d == 4) return 64;
        return 128;
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
                    weighted_deg += n_ij * std::sqrt(n_ij);
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
            double efficiency = weighted_deg / (denom * denom * trans_penalty);
            if (efficiency > max_efficiency) {
                max_efficiency = efficiency;
                best_dim = i;
            }
        }

        if (best_dim != -1 && max_efficiency > 0.0) return best_dim;
        return (last_split_dim + 1) % d;
    }

    int64_t select_cut_coordinate(int axis, const Box& cell, const std::vector<Box>& boxes) const {
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

    bool should_terminate_leaf(const Box& cell, const std::vector<Box>& boxes, uint64_t active_d2_faces) const {
        if (boxes.empty()) return true;
        int d = cell.dim();
        if ((int)boxes.size() <= get_threshold(d)) return true;
        if (active_d2_faces == 0) return true;
        return false;
    }

    Int128 evaluate_leaf(const Box& cell, const std::vector<Box>& boxes, int d) const {
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();
        if (d == 3) return evaluate_clean_cell_d3_trace(cell, boxes);
        return fast_kmp_sweep(boxes, d);
    }
};

#include "../solver/kmp_solver.hpp"

inline std::vector<Box> generate_random_uniform_boxes(int n, int d, uint64_t seed = 42) {
    FastRng rng(seed);
    const int64_t M = 1000000;
    std::vector<Box> boxes;
    boxes.reserve(n);
    for (int k = 0; k < n; ++k) {
        Box b;
        b.low.resize(d);
        b.high.resize(d);
        for (int i = 0; i < d; ++i) {
            int64_t u = rng.next_range(0, M - 50000);
            int64_t span = rng.next_range(50000, 300000);
            b.low[i] = u;
            b.high[i] = std::min(M, u + span);
        }
        boxes.push_back(b);
    }
    return boxes;
}

int main() {
    FastChampionPolicy fast_policy;
    std::cout << "Testing full grid d in {3,4,5,6} x n in {64,128,256,512,1024}..." << std::endl;

    for (int d : {3, 4, 5, 6}) {
        for (std::string fam : {"trellis", "hyperclique", "random"}) {
            for (int n : {64, 128, 256, 512, 1024}) {
                std::vector<Box> boxes;
                if (fam == "trellis") boxes = generate_trellis_weave(n, d, 42);
                else if (fam == "hyperclique") boxes = generate_hyperclique_gadget(n, d, 999);
                else boxes = generate_random_uniform_boxes(n, d, 777);

                StructuralCounters counters;
                auto t0 = std::chrono::high_resolution_clock::now();
                Int128 vol = KMPSolver::solve(boxes, counters, fast_policy);
                auto t1 = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

                std::cout << "[d=" << d << ", fam=" << std::setw(11) << fam << ", n=" << std::setw(4) << n << "] "
                          << "Faces: " << std::setw(8) << counters.total_d2_faces_processed
                          << " | Calls: " << std::setw(6) << counters.recursive_calls
                          << " | Time: " << std::setw(8) << std::fixed << std::setprecision(1) << ms << " ms"
                          << std::endl;
            }
        }
    }
    return 0;
}
