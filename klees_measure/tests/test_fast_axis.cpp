#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cassert>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"

using namespace kmp;

int select_axis_slow(const Box& cell, const std::vector<Box>& boxes, const std::vector<std::vector<uint32_t>>& d2_face_matrix, int last_dim) {
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
    return (last_dim + 1) % d;
}

int select_axis_fast(const Box& cell, const std::vector<Box>& boxes, const std::vector<std::vector<uint32_t>>& d2_face_matrix, int last_dim) {
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
                weighted_deg += n_ij * std::sqrt(n_ij);
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
    if (best_dim != -1 && max_efficiency > 0.0) return best_dim;
    return (last_dim + 1) % d;
}

int main() {
    auto boxes = generate_trellis_weave(1000, 6, 42);
    Box cell;
    cell.low.assign(6, 0);
    cell.high.assign(6, 1000000);
    std::vector<std::vector<uint32_t>> mat(6, std::vector<uint32_t>(6, 10));

    int a_slow = select_axis_slow(cell, boxes, mat, -1);
    int a_fast = select_axis_fast(cell, boxes, mat, -1);
    assert(a_slow == a_fast);
    std::cout << "Axis match verified: " << a_slow << " == " << a_fast << std::endl;
    return 0;
}
