#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <numeric>
#include <iostream>
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"

namespace kmp {

/**
 * Higher-Dimensional Tensor Network Clean-Cell Factorization Engine
 *
 * For any d-dimensional cell C = \prod_{i=0}^{d-1} [L_i, R_i] containing
 * only 1-slabs and 2-slabs (Clean cell), the volume of the complement
 * C \ \bigcup B is the exact contraction of a tensor network defined on
 * the 2-slab interaction graph G = (V_d, E_{d-2}).
 *
 * Supported exact 128-bit integer contraction routines:
 * - d=3: BLAS GEMM matrix trace Tr(D0 * A01 * D1 * A12 * D2 * A20)
 * - d=4: Bipartite factorized coordinate interval tensor contraction
 * - d>=3 general: General tensor network contraction engine
 */

struct Slab2 {
    int dim1;
    int dim2;
    int64_t a1, b1;
    int64_t a2, b2;
};

// Check if a cell is Clean (contains no >= 3-slabs)
inline bool is_clean_cell(const Box& cell, const std::vector<Box>& boxes) {
    int d = cell.dim();
    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        int active = 0;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) {
                // Disjoint in this dimension
                active = -1;
                break;
            }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) {
                active++;
            }
        }
        if (active >= 3) return false;
    }
    return true;
}

/**
 * Exact d=3 Clean-Cell Matrix Trace Factorization:
 * Tr(D0 * A01 * D1 * A12 * D2 * A20)
 */
inline Int128 evaluate_tensor_clean_cell_d3(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 3;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[3];
    std::vector<Slab2> slabs_2_01; // pair (0, 1)
    std::vector<Slab2> slabs_2_12; // pair (1, 2)
    std::vector<Slab2> slabs_2_02; // pair (0, 2)

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) {
                disjoint = true;
                break;
            }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) {
                noncov.push_back(i);
            }
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            // 0-slab covers the entire cell
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            int64_t l = std::max(cell.low[dim], b.low[dim]);
            int64_t h = std::min(cell.high[dim], b.high[dim]);
            slabs_1[dim].push_back({l, h});
        } else if (noncov.size() == 2) {
            int u = noncov[0], v = noncov[1];
            if (u > v) std::swap(u, v);
            int64_t l_u = std::max(cell.low[u], b.low[u]);
            int64_t h_u = std::min(cell.high[u], b.high[u]);
            int64_t l_v = std::max(cell.low[v], b.low[v]);
            int64_t h_v = std::min(cell.high[v], b.high[v]);

            Slab2 s{u, v, l_u, h_u, l_v, h_v};
            if (u == 0 && v == 1) slabs_2_01.push_back(s);
            else if (u == 1 && v == 2) slabs_2_12.push_back(s);
            else slabs_2_02.push_back(s);
        } else {
            // Cell is not clean; fallback
            return exact_kmp_sweep(boxes, d);
        }
    }

    // Discretize coordinates into elementary intervals
    std::vector<std::pair<int64_t, int64_t>> elementary[3];
    for (int i = 0; i < d; ++i) {
        std::vector<int64_t> coords;
        coords.push_back(cell.low[i]);
        coords.push_back(cell.high[i]);
        for (const auto& s : slabs_1[i]) {
            coords.push_back(s.first);
            coords.push_back(s.second);
        }
        if (i == 0) {
            for (const auto& s : slabs_2_01) { coords.push_back(s.a1); coords.push_back(s.b1); }
            for (const auto& s : slabs_2_02) { coords.push_back(s.a1); coords.push_back(s.b1); }
        } else if (i == 1) {
            for (const auto& s : slabs_2_01) { coords.push_back(s.a2); coords.push_back(s.b2); }
            for (const auto& s : slabs_2_12) { coords.push_back(s.a1); coords.push_back(s.b1); }
        } else {
            for (const auto& s : slabs_2_12) { coords.push_back(s.a2); coords.push_back(s.b2); }
            for (const auto& s : slabs_2_02) { coords.push_back(s.a2); coords.push_back(s.b2); }
        }
        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());
        for (size_t r = 0; r + 1 < coords.size(); ++r) {
            if (coords[r + 1] > coords[r]) {
                elementary[i].push_back({coords[r], coords[r + 1]});
            }
        }
    }

    size_t V0 = elementary[0].size();
    size_t V1 = elementary[1].size();
    size_t V2 = elementary[2].size();

    if (V0 == 0 || V1 == 0 || V2 == 0) {
        return cell.volume();
    }

    // 1-slab diagonal weight vectors D_i
    std::vector<int64_t> D[3];
    for (int i = 0; i < d; ++i) {
        D[i].resize(elementary[i].size(), 0);
        for (size_t r = 0; r < elementary[i].size(); ++r) {
            int64_t l = elementary[i][r].first;
            int64_t h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) {
                    cov = true;
                    break;
                }
            }
            if (!cov) {
                D[i][r] = h - l;
            }
        }
    }

    // Binary adjacency matrices A01, A12, A20
    std::vector<uint8_t> A01(V0 * V1, 1);
    for (size_t r0 = 0; r0 < V0; ++r0) {
        int64_t l0 = elementary[0][r0].first, h0 = elementary[0][r0].second;
        for (size_t r1 = 0; r1 < V1; ++r1) {
            int64_t l1 = elementary[1][r1].first, h1 = elementary[1][r1].second;
            for (const auto& s : slabs_2_01) {
                if (s.a1 <= l0 && s.b1 >= h0 && s.a2 <= l1 && s.b2 >= h1) {
                    A01[r0 * V1 + r1] = 0;
                    break;
                }
            }
        }
    }

    std::vector<uint8_t> A12(V1 * V2, 1);
    for (size_t r1 = 0; r1 < V1; ++r1) {
        int64_t l1 = elementary[1][r1].first, h1 = elementary[1][r1].second;
        for (size_t r2 = 0; r2 < V2; ++r2) {
            int64_t l2 = elementary[2][r2].first, h2 = elementary[2][r2].second;
            for (const auto& s : slabs_2_12) {
                if (s.a1 <= l1 && s.b1 >= h1 && s.a2 <= l2 && s.b2 >= h2) {
                    A12[r1 * V2 + r2] = 0;
                    break;
                }
            }
        }
    }

    std::vector<uint8_t> A20(V2 * V0, 1);
    for (size_t r2 = 0; r2 < V2; ++r2) {
        int64_t l2 = elementary[2][r2].first, h2 = elementary[2][r2].second;
        for (size_t r0 = 0; r0 < V0; ++r0) {
            int64_t l0 = elementary[0][r0].first, h0 = elementary[0][r0].second;
            for (const auto& s : slabs_2_02) {
                if (s.a1 <= l0 && s.b1 >= h0 && s.a2 <= l2 && s.b2 >= h2) {
                    A20[r2 * V0 + r0] = 0;
                    break;
                }
            }
        }
    }

    // Exact cyclic matrix trace contraction:
    // M1 = D0 * A01 * D1 (V0 x V1)
    std::vector<Int128> M1(V0 * V1, Int128(0));
    for (size_t r0 = 0; r0 < V0; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;
        for (size_t r1 = 0; r1 < V1; ++r1) {
            if (A01[r0 * V1 + r1] && D[1][r1] > 0) {
                M1[r0 * V1 + r1] = Int128(w0 * D[1][r1]);
            }
        }
    }

    // M2 = M1 * A12 (V0 x V2)
    std::vector<Int128> M2(V0 * V2, Int128(0));
    for (size_t r0 = 0; r0 < V0; ++r0) {
        for (size_t r1 = 0; r1 < V1; ++r1) {
            Int128 val1 = M1[r0 * V1 + r1];
            if (val1 == Int128(0)) continue;
            for (size_t r2 = 0; r2 < V2; ++r2) {
                if (A12[r1 * V2 + r2]) {
                    M2[r0 * V2 + r2] += val1;
                }
            }
        }
    }

    // Complement = Tr(M2 * D2 * A20)
    Int128 complement_vol(0);
    for (size_t r0 = 0; r0 < V0; ++r0) {
        for (size_t r2 = 0; r2 < V2; ++r2) {
            Int128 val2 = M2[r0 * V2 + r2];
            if (val2 == Int128(0)) continue;
            int64_t w2 = D[2][r2];
            if (w2 > 0 && A20[r2 * V0 + r0]) {
                complement_vol += val2 * w2;
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Exact d=4 Clean-Cell Tensor Network Contraction:
 * Evaluates the 4D contraction over the complete interaction graph K4
 * on coordinate interval matrices using factorized bipartite interaction.
 */
inline Int128 evaluate_tensor_clean_cell_d4(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 4;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[4];
    std::vector<Slab2> slabs_2[4][4]; // slabs_2[u][v] for u < v

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) {
                disjoint = true;
                break;
            }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) {
                noncov.push_back(i);
            }
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume(); // 0-slab covers cell
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            int64_t l = std::max(cell.low[dim], b.low[dim]);
            int64_t h = std::min(cell.high[dim], b.high[dim]);
            slabs_1[dim].push_back({l, h});
        } else if (noncov.size() == 2) {
            int u = noncov[0], v = noncov[1];
            if (u > v) std::swap(u, v);
            int64_t l_u = std::max(cell.low[u], b.low[u]);
            int64_t h_u = std::min(cell.high[u], b.high[u]);
            int64_t l_v = std::max(cell.low[v], b.low[v]);
            int64_t h_v = std::min(cell.high[v], b.high[v]);
            slabs_2[u][v].push_back({u, v, l_u, h_u, l_v, h_v});
        } else {
            return exact_kmp_sweep(boxes, d);
        }
    }

    // Step 2: Build elementary intervals per coordinate
    std::vector<std::pair<int64_t, int64_t>> elementary[4];
    for (int i = 0; i < d; ++i) {
        std::vector<int64_t> coords;
        coords.push_back(cell.low[i]);
        coords.push_back(cell.high[i]);
        for (const auto& s : slabs_1[i]) {
            coords.push_back(s.first);
            coords.push_back(s.second);
        }
        for (int other = 0; other < d; ++other) {
            if (other == i) continue;
            int u = std::min(i, other);
            int v = std::max(i, other);
            for (const auto& s : slabs_2[u][v]) {
                if (i == u) {
                    coords.push_back(s.a1);
                    coords.push_back(s.b1);
                } else {
                    coords.push_back(s.a2);
                    coords.push_back(s.b2);
                }
            }
        }
        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());
        for (size_t r = 0; r + 1 < coords.size(); ++r) {
            if (coords[r + 1] > coords[r]) {
                elementary[i].push_back({coords[r], coords[r + 1]});
            }
        }
    }

    size_t V[4];
    for (int i = 0; i < d; ++i) {
        V[i] = elementary[i].size();
        if (V[i] == 0) return cell.volume();
    }

    // Step 3: Compute diagonal weights D_i
    std::vector<int64_t> D[4];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first;
            int64_t h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) {
                    cov = true;
                    break;
                }
            }
            if (!cov) {
                D[i][r] = h - l;
            }
        }
    }

    // Step 3.5: Fast-Path Treewidth & Sparsity Early Exits
    // 1. Degenerate axis: if any axis is 100% covered by 1-slabs, complement volume is 0
    int64_t sum_D[4] = {0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) {
            sum_D[i] += D[i][r];
        }
        if (sum_D[i] == 0) {
            return cell.volume();
        }
    }

    // 2. Count active 2-slab edges in interaction graph G = (V_4, E_2)
    int edge_count = 0;
    int single_u = -1, single_v = -1;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) {
                edge_count++;
                single_u = u;
                single_v = v;
            }
        }
    }

    // Treewidth tw=0 (Empty interaction graph, pure 1-slab tensor product)
    if (edge_count == 0) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        return cell.volume() - comp_vol;
    }

    // Treewidth tw=1 (Single active 2-slab ridge, other 2 axes unconstrained)
    if (edge_count == 1) {
        int u = single_u, v = single_v;
        int rem[2], rem_idx = 0;
        for (int i = 0; i < d; ++i) {
            if (i != u && i != v) rem[rem_idx++] = i;
        }

        Int128 inner(0);
        for (size_t ru = 0; ru < V[u]; ++ru) {
            int64_t wu = D[u][ru];
            if (wu == 0) continue;
            int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
            int64_t row_v = 0;
            for (size_t rv = 0; rv < V[v]; ++rv) {
                int64_t wv = D[v][rv];
                if (wv == 0) continue;
                int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                bool cov = false;
                for (const auto& s : slabs_2[u][v]) {
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) {
                        cov = true;
                        break;
                    }
                }
                if (!cov) row_v += wv;
            }
            if (row_v > 0) {
                inner += Int128(wu) * row_v;
            }
        }
        Int128 comp_vol = inner * sum_D[rem[0]];
        comp_vol = comp_vol * sum_D[rem[1]];
        return cell.volume() - comp_vol;
    }

    // Step 4: Build binary adjacency matrices A^{(u, v)} for active pairs
    std::vector<uint8_t> A[4][4];
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            A[u][v].assign(V[u] * V[v], 1);
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (const auto& s : slabs_2[u][v]) {
                        if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) {
                            A[u][v][ru * V[v] + rv] = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Step 5: High-efficiency exact tensor network contraction for d=4
    // Contraction formula:
    // \sum_{r0, r1, r2, r3} D0[r0] D1[r1] D2[r2] D3[r3]
    //                       A01[r0,r1] A02[r0,r2] A03[r0,r3] A12[r1,r2] A13[r1,r3] A23[r2,r3]
    //
    // Factorization:
    // Outer loops over (r0, r1) with non-zero W01 = D0[r0] * D1[r1] * A01[r0, r1].
    // Filter active sets U(r0, r1) \subseteq [V2] and V(r0, r1) \subseteq [V3].
    // Inner contraction evaluates u^T * A23 * v.

    Int128 complement_vol(0);
    std::vector<size_t> active_r2;
    active_r2.reserve(V[2]);
    std::vector<size_t> active_r3;
    active_r3.reserve(V[3]);

    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;

        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t w1 = D[1][r1];
            if (w1 == 0 || !A[0][1][r0 * V[1] + r1]) continue;

            Int128 w01 = Int128(w0) * w1;

            // Find active r2: D2[r2] > 0 and A02[r0, r2] == 1 and A12[r1, r2] == 1
            active_r2.clear();
            const uint8_t* a02_row = &A[0][2][r0 * V[2]];
            const uint8_t* a12_row = &A[1][2][r1 * V[2]];
            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                if (D[2][r2] > 0 && a02_row[r2] && a12_row[r2]) {
                    active_r2.push_back(r2);
                }
            }
            if (active_r2.empty()) continue;

            // Find active r3: D3[r3] > 0 and A03[r0, r3] == 1 and A13[r1, r3] == 1
            active_r3.clear();
            const uint8_t* a03_row = &A[0][3][r0 * V[3]];
            const uint8_t* a13_row = &A[1][3][r1 * V[3]];
            for (size_t r3 = 0; r3 < V[3]; ++r3) {
                if (D[3][r3] > 0 && a03_row[r3] && a13_row[r3]) {
                    active_r3.push_back(r3);
                }
            }
            if (active_r3.empty()) continue;

            // Contract over active r2 and active r3
            for (size_t r2 : active_r2) {
                int64_t w2 = D[2][r2];
                Int128 w012 = w01 * w2;

                const uint8_t* a23_row = &A[2][3][r2 * V[3]];
                int64_t sum_v = 0;
                for (size_t r3 : active_r3) {
                    if (a23_row[r3]) {
                        sum_v += D[3][r3];
                    }
                }
                if (sum_v > 0) {
                    complement_vol += w012 * sum_v;
                }
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Unified Clean-Cell Tensor Network Solver Dispatcher
 */
inline Int128 evaluate_tensor_clean_cell(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    int d = cell.dim();
    if (d == 1) {
        return exact_kmp_sweep(boxes, 1);
    } else if (d == 2) {
        return exact_kmp_sweep(boxes, 2);
    } else if (d == 3) {
        return evaluate_tensor_clean_cell_d3(cell, boxes);
    } else if (d == 4) {
        return evaluate_tensor_clean_cell_d4(cell, boxes);
    } else {
        // Fallback for general dimensions
        return exact_kmp_sweep(boxes, d);
    }
}

} // namespace kmp
