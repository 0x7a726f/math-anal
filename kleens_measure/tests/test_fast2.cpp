#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"

using namespace kmp;

// Include stack sweep
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

inline Int128 stack_sweep(const StackBox* boxes, size_t n, int d) {
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

inline Int128 stack_sweep_wrapper(const std::vector<Box>& boxes, int d) {
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

#include "../solver/clean_cell_trace.hpp"

struct FastPolicy2 {
    std::string id = "H-51 Champion";

    int get_threshold(int d) const {
        if (d >= 6) return 20;
        if (d == 5) return 32;
        if (d == 4) return 64;
        return 128;
    }

    int select_split_axis(const Box& cell, const std::vector<Box>& boxes, const std::vector<std::vector<uint32_t>>& d2_face_matrix, int last_split_dim) const {
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
        return stack_sweep_wrapper(boxes, d);
    }
};

#include "../solver/kmp_solver.hpp"

int main() {
    FastPolicy2 policy;
    std::cout << "Testing FastPolicy2 on d=5,6 for n=64, 128, 256, 512, 1024..." << std::endl;
    for (int d : {5, 6}) {
        for (int n : {64, 128, 256, 512, 1024}) {
            auto boxes = generate_trellis_weave(n, d, 42);
            StructuralCounters counters;
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 vol = KMPSolver::solve(boxes, counters, policy);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "d=" << d << " n=" << std::setw(4) << n
                      << " | time: " << std::setw(8) << std::fixed << std::setprecision(1) << ms << " ms"
                      << " | calls: " << std::setw(6) << counters.recursive_calls
                      << " | faces: " << std::setw(8) << counters.total_d2_faces_processed
                      << std::endl;
            if (ms > 5000.0) break;
        }
    }
    return 0;
}
