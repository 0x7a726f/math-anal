#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <numeric>
#include <iostream>
#include <immintrin.h>
#include <functional>
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"

namespace kmp {

/**
 * ============================================================================
 * Higher-Dimensional Tensor Network Clean-Cell Factorization Engine
 * Exact 128-bit Integer Arithmetic with SIMD AVX2 Bitmask Acceleration
 * ============================================================================
 *
 * For any d-dimensional cell C = \prod_{i=0}^{d-1} [L_i, R_i] containing
 * only 1-slabs and 2-slabs (Clean cell), the volume of the complement
 * C \ \bigcup B is the exact contraction of a tensor network defined on
 * the 2-slab interaction graph G = (V_d, E_{d-2}).
 *
 * Supported exact 128-bit integer contraction routines:
 * - d=3: Cyclic matrix trace Tr(D0 * A01 * D1 * A12 * D2 * A20) via BLAS GEMM
 * - d=4: SIMD/Bitmask accelerated bipartite factorized contraction
 * - d=5: SIMD/Bitmask vectorized inner contraction over K5 (10 edges)
 * - d=6: SIMD/Bitmask tripartite compound block trace Tr(diag(WA)*MAB*diag(WB)*MBC*diag(WC)*MCA)
 */

struct Slab2 {
    int dim1;
    int dim2;
    int64_t a1, b1;
    int64_t a2, b2;
};

struct Slab3 {
    int dim1, dim2, dim3;
    int64_t a1, b1;
    int64_t a2, b2;
    int64_t a3, b3;
};

// Portable trailing zero count and lowest-bit reset for 64-bit words
inline int count_trailing_zeros_u64(uint64_t x) {
#if defined(_MSC_VER)
    unsigned long index;
    #if defined(_M_X64)
    _BitScanForward64(&index, x);
    return (int)index;
    #else
    if ((uint32_t)x) {
        _BitScanForward(&index, (uint32_t)x);
        return (int)index;
    } else {
        _BitScanForward(&index, (uint32_t)(x >> 32));
        return (int)index + 32;
    }
    #endif
#else
    return __builtin_ctzll(x);
#endif
}

inline uint64_t reset_lowest_set_bit_u64(uint64_t x) {
    return x & (x - 1ULL);
}

/**
 * SIMD-aligned Bitmask with AVX2 vectorized bitwise AND and testz.
 */
struct SimdBitmask {
    std::vector<uint64_t> words;
    size_t num_bits;

    SimdBitmask() : num_bits(0) {}
    SimdBitmask(size_t n, bool set_all = false) : num_bits(n) {
        size_t n_words = (n + 63) / 64;
        if (n_words % 4 != 0) n_words += 4 - (n_words % 4); // 256-bit aligned
        if (n_words == 0) n_words = 4;
        words.assign(n_words, set_all ? ~0ULL : 0ULL);
        if (set_all && n > 0) {
            size_t rem = n % 64;
            size_t last_full = n / 64;
            if (rem != 0 && last_full < words.size()) {
                words[last_full] = (1ULL << rem) - 1ULL;
            }
            for (size_t w = last_full + (rem != 0 ? 1 : 0); w < words.size(); ++w) {
                words[w] = 0ULL;
            }
        }
    }

    void set(size_t i) {
        words[i / 64] |= (1ULL << (i % 64));
    }

    void reset(size_t i) {
        words[i / 64] &= ~(1ULL << (i % 64));
    }

    bool test(size_t i) const {
        return (words[i / 64] >> (i % 64)) & 1ULL;
    }

    bool is_zero() const {
        size_t n_words = words.size();
#if defined(__AVX2__)
        for (size_t w = 0; w < n_words; w += 4) {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&words[w]));
            if (!_mm256_testz_si256(v, v)) return false;
        }
        return true;
#else
        for (size_t w = 0; w < n_words; ++w) {
            if (words[w] != 0ULL) return false;
        }
        return true;
#endif
    }
};

inline SimdBitmask bitwise_and(const SimdBitmask& a, const SimdBitmask& b) {
    SimdBitmask res(a.num_bits, false);
    size_t n_words = a.words.size();
#if defined(__AVX2__)
    for (size_t w = 0; w < n_words; w += 4) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&a.words[w]));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&b.words[w]));
        __m256i vc = _mm256_and_si256(va, vb);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&res.words[w]), vc);
    }
#else
    for (size_t w = 0; w < n_words; ++w) {
        res.words[w] = a.words[w] & b.words[w];
    }
#endif
    return res;
}

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

// Check if a cell is Hypergraph-Clean (contains no >= 4-slabs; only 0, 1, 2, and 3-slabs)
inline bool is_hypergraph_clean_cell(const Box& cell, const std::vector<Box>& boxes) {
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
        if (active >= 4) return false;
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
 * SIMD/Bitmask accelerated factorized bipartite interaction.
 */
inline Int128 evaluate_tensor_clean_cell_d4(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 4;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[4];
    std::vector<Slab2> slabs_2[4][4];

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
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
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
                if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                else { coords.push_back(s.a2); coords.push_back(s.b2); }
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

    std::vector<int64_t> D[4];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
        }
    }

    // Fast-Path: Degenerate Axis
    int64_t sum_D[4] = {0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) sum_D[i] += D[i][r];
        if (sum_D[i] == 0) return cell.volume();
    }

    // Fast-Path: Treewidth tw=0 (Empty interaction graph)
    int edge_count = 0;
    int single_u = -1, single_v = -1;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) { edge_count++; single_u = u; single_v = v; }
        }
    }

    if (edge_count == 0) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        return cell.volume() - comp_vol;
    }

    // Fast-Path: Treewidth tw=1 (Single active 2-slab pair)
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
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) { cov = true; break; }
                }
                if (!cov) row_v += wv;
            }
            if (row_v > 0) inner += Int128(wu) * row_v;
        }
        Int128 comp_vol = inner * sum_D[rem[0]];
        comp_vol = comp_vol * sum_D[rem[1]];
        return cell.volume() - comp_vol;
    }

    // SIMD Bitmask Representation for dimension 3
    SimdBitmask U3(V[3], false);
    for (size_t r3 = 0; r3 < V[3]; ++r3) {
        if (D[3][r3] > 0) U3.set(r3);
    }

    std::vector<SimdBitmask> A03(V[0], SimdBitmask(V[3], true));
    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t l0 = elementary[0][r0].first, h0 = elementary[0][r0].second;
        for (size_t r3 = 0; r3 < V[3]; ++r3) {
            int64_t l3 = elementary[3][r3].first, h3 = elementary[3][r3].second;
            for (const auto& s : slabs_2[0][3]) {
                if (s.a1 <= l0 && s.b1 >= h0 && s.a2 <= l3 && s.b2 >= h3) { A03[r0].reset(r3); break; }
            }
        }
    }

    std::vector<SimdBitmask> A13(V[1], SimdBitmask(V[3], true));
    for (size_t r1 = 0; r1 < V[1]; ++r1) {
        int64_t l1 = elementary[1][r1].first, h1 = elementary[1][r1].second;
        for (size_t r3 = 0; r3 < V[3]; ++r3) {
            int64_t l3 = elementary[3][r3].first, h3 = elementary[3][r3].second;
            for (const auto& s : slabs_2[1][3]) {
                if (s.a1 <= l1 && s.b1 >= h1 && s.a2 <= l3 && s.b2 >= h3) { A13[r1].reset(r3); break; }
            }
        }
    }

    std::vector<SimdBitmask> A23(V[2], SimdBitmask(V[3], true));
    for (size_t r2 = 0; r2 < V[2]; ++r2) {
        int64_t l2 = elementary[2][r2].first, h2 = elementary[2][r2].second;
        for (size_t r3 = 0; r3 < V[3]; ++r3) {
            int64_t l3 = elementary[3][r3].first, h3 = elementary[3][r3].second;
            for (const auto& s : slabs_2[2][3]) {
                if (s.a1 <= l2 && s.b1 >= h2 && s.a2 <= l3 && s.b2 >= h3) { A23[r2].reset(r3); break; }
            }
        }
    }

    std::vector<uint8_t> A01(V[0] * V[1], 1);
    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t l0 = elementary[0][r0].first, h0 = elementary[0][r0].second;
        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t l1 = elementary[1][r1].first, h1 = elementary[1][r1].second;
            for (const auto& s : slabs_2[0][1]) {
                if (s.a1 <= l0 && s.b1 >= h0 && s.a2 <= l1 && s.b2 >= h1) { A01[r0 * V[1] + r1] = 0; break; }
            }
        }
    }

    std::vector<uint8_t> A02(V[0] * V[2], 1);
    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t l0 = elementary[0][r0].first, h0 = elementary[0][r0].second;
        for (size_t r2 = 0; r2 < V[2]; ++r2) {
            int64_t l2 = elementary[2][r2].first, h2 = elementary[2][r2].second;
            for (const auto& s : slabs_2[0][2]) {
                if (s.a1 <= l0 && s.b1 >= h0 && s.a2 <= l2 && s.b2 >= h2) { A02[r0 * V[2] + r2] = 0; break; }
            }
        }
    }

    std::vector<uint8_t> A12(V[1] * V[2], 1);
    for (size_t r1 = 0; r1 < V[1]; ++r1) {
        int64_t l1 = elementary[1][r1].first, h1 = elementary[1][r1].second;
        for (size_t r2 = 0; r2 < V[2]; ++r2) {
            int64_t l2 = elementary[2][r2].first, h2 = elementary[2][r2].second;
            for (const auto& s : slabs_2[1][2]) {
                if (s.a1 <= l1 && s.b1 >= h1 && s.a2 <= l2 && s.b2 >= h2) { A12[r1 * V[2] + r2] = 0; break; }
            }
        }
    }

    Int128 complement_vol(0);
    std::vector<size_t> act_r2;
    act_r2.reserve(V[2]);

    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;
        SimdBitmask mask_03 = bitwise_and(A03[r0], U3);
        if (mask_03.is_zero()) continue;

        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t w1 = D[1][r1];
            if (w1 == 0 || !A01[r0 * V[1] + r1]) continue;

            SimdBitmask mask_013 = bitwise_and(mask_03, A13[r1]);
            if (mask_013.is_zero()) continue;

            Int128 w01 = Int128(w0) * w1;
            act_r2.clear();
            const uint8_t* a02_row = &A02[r0 * V[2]];
            const uint8_t* a12_row = &A12[r1 * V[2]];
            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                if (D[2][r2] > 0 && a02_row[r2] && a12_row[r2]) act_r2.push_back(r2);
            }
            if (act_r2.empty()) continue;

            for (size_t r2 : act_r2) {
                Int128 w012 = w01 * D[2][r2];
                SimdBitmask mask_final = bitwise_and(mask_013, A23[r2]);
                if (mask_final.is_zero()) continue;

                int64_t sum_v = 0;
                for (size_t w_idx = 0; w_idx < mask_final.words.size(); ++w_idx) {
                    uint64_t word = mask_final.words[w_idx];
                    while (word) {
                        int bit = count_trailing_zeros_u64(word);
                        size_t r3 = w_idx * 64 + bit;
                        if (r3 < V[3]) sum_v += D[3][r3];
                        word = reset_lowest_set_bit_u64(word);
                    }
                }
                if (sum_v > 0) complement_vol += w012 * sum_v;
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Exact d=5 Clean-Cell Tensor Network Contraction:
 * Vectorized AVX2 bitmask inner contraction over K5 (10 edges).
 */
inline Int128 evaluate_tensor_clean_cell_d5(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 5;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[5];
    std::vector<Slab2> slabs_2[5][5];

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) { disjoint = true; break; }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) noncov.push_back(i);
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
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

    std::vector<std::pair<int64_t, int64_t>> elementary[5];
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
                if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                else { coords.push_back(s.a2); coords.push_back(s.b2); }
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

    size_t V[5];
    for (int i = 0; i < d; ++i) {
        V[i] = elementary[i].size();
        if (V[i] == 0) return cell.volume();
    }

    std::vector<int64_t> D[5];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
        }
    }

    // Fast-Path: Degenerate Axis
    int64_t sum_D[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) sum_D[i] += D[i][r];
        if (sum_D[i] == 0) return cell.volume();
    }

    // Fast-Path: Empty graph (tw=0)
    int edge_count = 0;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) edge_count++;
        }
    }
    if (edge_count == 0) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        comp_vol = comp_vol * sum_D[4];
        return cell.volume() - comp_vol;
    }

    // Build SIMD Bitmask for coordinate 4
    SimdBitmask U4(V[4], false);
    for (size_t r4 = 0; r4 < V[4]; ++r4) {
        if (D[4][r4] > 0) U4.set(r4);
    }

    std::vector<SimdBitmask> A_to_4[4];
    for (int u = 0; u < 4; ++u) {
        A_to_4[u].assign(V[u], SimdBitmask(V[4], true));
        for (size_t ru = 0; ru < V[u]; ++ru) {
            int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
            for (size_t r4 = 0; r4 < V[4]; ++r4) {
                int64_t l4 = elementary[4][r4].first, h4 = elementary[4][r4].second;
                for (const auto& s : slabs_2[u][4]) {
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= l4 && s.b2 >= h4) { A_to_4[u][ru].reset(r4); break; }
                }
            }
        }
    }

    // Build 2D tables for remaining pairs (u, v) where u < v < 4
    std::vector<uint8_t> A_table[4][4];
    for (int u = 0; u < 4; ++u) {
        for (int v = u + 1; v < 4; ++v) {
            A_table[u][v].assign(V[u] * V[v], 1);
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (const auto& s : slabs_2[u][v]) {
                        if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) { A_table[u][v][ru * V[v] + rv] = 0; break; }
                    }
                }
            }
        }
    }

    Int128 complement_vol(0);
    std::vector<size_t> act_r2;
    act_r2.reserve(V[2]);
    std::vector<size_t> act_r3;
    act_r3.reserve(V[3]);

    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;
        SimdBitmask mask_04 = bitwise_and(A_to_4[0][r0], U4);
        if (mask_04.is_zero()) continue;

        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t w1 = D[1][r1];
            if (w1 == 0 || !A_table[0][1][r0 * V[1] + r1]) continue;

            SimdBitmask mask_014 = bitwise_and(mask_04, A_to_4[1][r1]);
            if (mask_014.is_zero()) continue;

            Int128 w01 = Int128(w0) * w1;

            act_r2.clear();
            const uint8_t* a02_row = &A_table[0][2][r0 * V[2]];
            const uint8_t* a12_row = &A_table[1][2][r1 * V[2]];
            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                if (D[2][r2] > 0 && a02_row[r2] && a12_row[r2]) act_r2.push_back(r2);
            }
            if (act_r2.empty()) continue;

            act_r3.clear();
            const uint8_t* a03_row = &A_table[0][3][r0 * V[3]];
            const uint8_t* a13_row = &A_table[1][3][r1 * V[3]];
            for (size_t r3 = 0; r3 < V[3]; ++r3) {
                if (D[3][r3] > 0 && a03_row[r3] && a13_row[r3]) act_r3.push_back(r3);
            }
            if (act_r3.empty()) continue;

            for (size_t r2 : act_r2) {
                Int128 w012 = w01 * D[2][r2];
                SimdBitmask mask_0124 = bitwise_and(mask_014, A_to_4[2][r2]);
                if (mask_0124.is_zero()) continue;

                const uint8_t* a23_row = &A_table[2][3][r2 * V[3]];
                for (size_t r3 : act_r3) {
                    if (!a23_row[r3]) continue;
                    Int128 w0123 = w012 * D[3][r3];
                    SimdBitmask mask_final = bitwise_and(mask_0124, A_to_4[3][r3]);
                    if (mask_final.is_zero()) continue;

                    int64_t sum_4 = 0;
                    for (size_t w_idx = 0; w_idx < mask_final.words.size(); ++w_idx) {
                        uint64_t word = mask_final.words[w_idx];
                        while (word) {
                            int bit = count_trailing_zeros_u64(word);
                            size_t r4 = w_idx * 64 + bit;
                            if (r4 < V[4]) sum_4 += D[4][r4];
                            word = reset_lowest_set_bit_u64(word);
                        }
                    }
                    if (sum_4 > 0) complement_vol += w0123 * sum_4;
                }
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Exact d=6 Clean-Cell Tensor Network Contraction:
 * Tripartite Compound Block Decomposition (G_A={0,1}, G_B={2,3}, G_C={4,5})
 * with SIMD bitmask cyclic trace contraction.
 */
inline Int128 evaluate_tensor_clean_cell_d6(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 6;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[6];
    std::vector<Slab2> slabs_2[6][6];

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) { disjoint = true; break; }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) noncov.push_back(i);
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
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

    std::vector<std::pair<int64_t, int64_t>> elementary[6];
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
                if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                else { coords.push_back(s.a2); coords.push_back(s.b2); }
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

    size_t V[6];
    for (int i = 0; i < d; ++i) {
        V[i] = elementary[i].size();
        if (V[i] == 0) return cell.volume();
    }

    std::vector<int64_t> D[6];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
        }
    }

    // Fast-Path: Degenerate Axis
    int64_t sum_D[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) sum_D[i] += D[i][r];
        if (sum_D[i] == 0) return cell.volume();
    }

    // Fast-Path: Empty graph (tw=0)
    int edge_count = 0;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) edge_count++;
        }
    }
    if (edge_count == 0) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        comp_vol = comp_vol * sum_D[4];
        comp_vol = comp_vol * sum_D[5];
        return cell.volume() - comp_vol;
    }

    // Pairwise adjacency tables
    auto get_compat = [&](int u, int v, size_t ru, size_t rv) -> bool {
        int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
        int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
        for (const auto& s : slabs_2[u][v]) {
            if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) return false;
        }
        return true;
    };

    struct State2 {
        size_t r0, r1;
        int64_t w0, w1;
    };

    // Group A: {0, 1}
    std::vector<State2> states_A;
    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        if (D[0][r0] == 0) continue;
        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            if (D[1][r1] > 0 && get_compat(0, 1, r0, r1)) {
                states_A.push_back({r0, r1, D[0][r0], D[1][r1]});
            }
        }
    }
    if (states_A.empty()) return cell.volume();

    // Group B: {2, 3}
    std::vector<State2> states_B;
    for (size_t r2 = 0; r2 < V[2]; ++r2) {
        if (D[2][r2] == 0) continue;
        for (size_t r3 = 0; r3 < V[3]; ++r3) {
            if (D[3][r3] > 0 && get_compat(2, 3, r2, r3)) {
                states_B.push_back({r2, r3, D[2][r2], D[3][r3]});
            }
        }
    }
    if (states_B.empty()) return cell.volume();

    // Group C: {4, 5}
    std::vector<State2> states_C;
    for (size_t r4 = 0; r4 < V[4]; ++r4) {
        if (D[4][r4] == 0) continue;
        for (size_t r5 = 0; r5 < V[5]; ++r5) {
            if (D[5][r5] > 0 && get_compat(4, 5, r4, r5)) {
                states_C.push_back({r4, r5, D[4][r4], D[5][r5]});
            }
        }
    }
    if (states_C.empty()) return cell.volume();

    size_t NA = states_A.size();
    size_t NB = states_B.size();
    size_t NC = states_C.size();

    // M_AB bitmasks: size NA, each mask over NB
    std::vector<SimdBitmask> M_AB(NA, SimdBitmask(NB, false));
    for (size_t p = 0; p < NA; ++p) {
        size_t r0 = states_A[p].r0, r1 = states_A[p].r1;
        for (size_t q = 0; q < NB; ++q) {
            size_t r2 = states_B[q].r0, r3 = states_B[q].r1;
            if (get_compat(0, 2, r0, r2) && get_compat(0, 3, r0, r3) &&
                get_compat(1, 2, r1, r2) && get_compat(1, 3, r1, r3)) {
                M_AB[p].set(q);
            }
        }
    }

    // M_BC_col bitmasks: size NC, each mask over NB
    std::vector<SimdBitmask> M_BC_col(NC, SimdBitmask(NB, false));
    for (size_t s = 0; s < NC; ++s) {
        size_t r4 = states_C[s].r0, r5 = states_C[s].r1;
        for (size_t q = 0; q < NB; ++q) {
            size_t r2 = states_B[q].r0, r3 = states_B[q].r1;
            if (get_compat(2, 4, r2, r4) && get_compat(2, 5, r2, r5) &&
                get_compat(3, 4, r3, r4) && get_compat(3, 5, r3, r5)) {
                M_BC_col[s].set(q);
            }
        }
    }

    Int128 complement_vol(0);

    for (size_t p = 0; p < NA; ++p) {
        if (M_AB[p].is_zero()) continue;
        size_t r0 = states_A[p].r0, r1 = states_A[p].r1;
        Int128 w_A = Int128(states_A[p].w0) * states_A[p].w1;

        for (size_t s = 0; s < NC; ++s) {
            size_t r4 = states_C[s].r0, r5 = states_C[s].r1;
            if (!get_compat(0, 4, r0, r4) || !get_compat(1, 4, r1, r4) ||
                !get_compat(0, 5, r0, r5) || !get_compat(1, 5, r1, r5)) {
                continue;
            }

            SimdBitmask mask_q = bitwise_and(M_AB[p], M_BC_col[s]);
            if (mask_q.is_zero()) continue;

            Int128 w_AC = w_A * states_C[s].w0;
            w_AC = w_AC * states_C[s].w1;

            int64_t sum_wB = 0;
            for (size_t w_idx = 0; w_idx < mask_q.words.size(); ++w_idx) {
                uint64_t word = mask_q.words[w_idx];
                while (word) {
                    int bit = count_trailing_zeros_u64(word);
                    size_t q = w_idx * 64 + bit;
                    if (q < NB) {
                        sum_wB += states_B[q].w0 * states_B[q].w1;
                    }
                    word = reset_lowest_set_bit_u64(word);
                }
            }

            if (sum_wB > 0) {
                complement_vol += w_AC * sum_wB;
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Exact d=7 Clean-Cell Tensor Network Contraction:
 * Complete interaction graph K7 (21 edges).
 * Hierarchical prefix {0, 1, 2} with 4D inner AVX2 SIMD bitmask contraction.
 */
inline Int128 evaluate_tensor_clean_cell_d7(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 7;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[7];
    std::vector<Slab2> slabs_2[7][7];

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) { disjoint = true; break; }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) noncov.push_back(i);
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
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

    std::vector<std::pair<int64_t, int64_t>> elementary[7];
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
                if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                else { coords.push_back(s.a2); coords.push_back(s.b2); }
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

    size_t V[7];
    for (int i = 0; i < d; ++i) {
        V[i] = elementary[i].size();
        if (V[i] == 0) return cell.volume();
    }

    std::vector<int64_t> D[7];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
        }
    }

    // Fast-Path: Degenerate Axis
    int64_t sum_D[7] = {0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) sum_D[i] += D[i][r];
        if (sum_D[i] == 0) return cell.volume();
    }

    // Fast-Path: Empty graph (tw=0)
    int edge_count = 0;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) edge_count++;
        }
    }
    if (edge_count == 0) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        comp_vol = comp_vol * sum_D[4];
        comp_vol = comp_vol * sum_D[5];
        comp_vol = comp_vol * sum_D[6];
        return cell.volume() - comp_vol;
    }

    // Build SIMD Bitmask for coordinate 6
    SimdBitmask U6(V[6], false);
    for (size_t r6 = 0; r6 < V[6]; ++r6) {
        if (D[6][r6] > 0) U6.set(r6);
    }

    std::vector<SimdBitmask> A_to_6[6];
    for (int u = 0; u < 6; ++u) {
        A_to_6[u].assign(V[u], SimdBitmask(V[6], true));
        for (size_t ru = 0; ru < V[u]; ++ru) {
            int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
            for (size_t r6 = 0; r6 < V[6]; ++r6) {
                int64_t l6 = elementary[6][r6].first, h6 = elementary[6][r6].second;
                for (const auto& s : slabs_2[u][6]) {
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= l6 && s.b2 >= h6) { A_to_6[u][ru].reset(r6); break; }
                }
            }
        }
    }

    // Pairwise tables between dimensions u, v with u < v < 6
    std::vector<uint8_t> A_table[6][6];
    for (int u = 0; u < 6; ++u) {
        for (int v = u + 1; v < 6; ++v) {
            A_table[u][v].assign(V[u] * V[v], 1);
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (const auto& s : slabs_2[u][v]) {
                        if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) { A_table[u][v][ru * V[v] + rv] = 0; break; }
                    }
                }
            }
        }
    }

    Int128 complement_vol(0);
    std::vector<size_t> act_r3;
    act_r3.reserve(V[3]);
    std::vector<size_t> act_r4;
    act_r4.reserve(V[4]);
    std::vector<size_t> act_r5;
    act_r5.reserve(V[5]);

    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;
        SimdBitmask mask_06 = bitwise_and(A_to_6[0][r0], U6);
        if (mask_06.is_zero()) continue;

        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t w1 = D[1][r1];
            if (w1 == 0 || !A_table[0][1][r0 * V[1] + r1]) continue;

            SimdBitmask mask_016 = bitwise_and(mask_06, A_to_6[1][r1]);
            if (mask_016.is_zero()) continue;

            Int128 w01 = Int128(w0) * w1;

            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                int64_t w2 = D[2][r2];
                if (w2 == 0 || !A_table[0][2][r0 * V[2] + r2] || !A_table[1][2][r1 * V[2] + r2]) continue;

                SimdBitmask mask_0126 = bitwise_and(mask_016, A_to_6[2][r2]);
                if (mask_0126.is_zero()) continue;

                Int128 w012 = w01 * w2;

                // Filter active r3, r4, r5 compatible with (r0, r1, r2)
                act_r3.clear();
                const uint8_t* a03 = &A_table[0][3][r0 * V[3]];
                const uint8_t* a13 = &A_table[1][3][r1 * V[3]];
                const uint8_t* a23 = &A_table[2][3][r2 * V[3]];
                for (size_t r3 = 0; r3 < V[3]; ++r3) {
                    if (D[3][r3] > 0 && a03[r3] && a13[r3] && a23[r3]) act_r3.push_back(r3);
                }
                if (act_r3.empty()) continue;

                act_r4.clear();
                const uint8_t* a04 = &A_table[0][4][r0 * V[4]];
                const uint8_t* a14 = &A_table[1][4][r1 * V[4]];
                const uint8_t* a24 = &A_table[2][4][r2 * V[4]];
                for (size_t r4 = 0; r4 < V[4]; ++r4) {
                    if (D[4][r4] > 0 && a04[r4] && a14[r4] && a24[r4]) act_r4.push_back(r4);
                }
                if (act_r4.empty()) continue;

                act_r5.clear();
                const uint8_t* a05 = &A_table[0][5][r0 * V[5]];
                const uint8_t* a15 = &A_table[1][5][r1 * V[5]];
                const uint8_t* a25 = &A_table[2][5][r2 * V[5]];
                for (size_t r5 = 0; r5 < V[5]; ++r5) {
                    if (D[5][r5] > 0 && a05[r5] && a15[r5] && a25[r5]) act_r5.push_back(r5);
                }
                if (act_r5.empty()) continue;

                // Inner 4D contraction: r3, r4, r5, r6
                for (size_t r3 : act_r3) {
                    SimdBitmask mask_01236 = bitwise_and(mask_0126, A_to_6[3][r3]);
                    if (mask_01236.is_zero()) continue;
                    Int128 w0123 = w012 * D[3][r3];

                    const uint8_t* a34 = &A_table[3][4][r3 * V[4]];
                    const uint8_t* a35 = &A_table[3][5][r3 * V[5]];

                    for (size_t r4 : act_r4) {
                        if (!a34[r4]) continue;
                        SimdBitmask mask_012346 = bitwise_and(mask_01236, A_to_6[4][r4]);
                        if (mask_012346.is_zero()) continue;
                        Int128 w01234 = w0123 * D[4][r4];

                        const uint8_t* a45 = &A_table[4][5][r4 * V[5]];

                        for (size_t r5 : act_r5) {
                            if (!a35[r5] || !a45[r5]) continue;
                            SimdBitmask mask_final = bitwise_and(mask_012346, A_to_6[5][r5]);
                            if (mask_final.is_zero()) continue;

                            Int128 w012345 = w01234 * D[5][r5];

                            int64_t sum_6 = 0;
                            for (size_t w_idx = 0; w_idx < mask_final.words.size(); ++w_idx) {
                                uint64_t word = mask_final.words[w_idx];
                                while (word) {
                                    int bit = count_trailing_zeros_u64(word);
                                    size_t r6 = w_idx * 64 + bit;
                                    if (r6 < V[6]) sum_6 += D[6][r6];
                                    word = reset_lowest_set_bit_u64(word);
                                }
                            }
                            if (sum_6 > 0) {
                                complement_vol += w012345 * sum_6;
                            }
                        }
                    }
                }
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Exact General d-Dimensional Clean-Cell Tensor Network Contraction:
 * Arbitrary d >= 3.
 * Recursive DFS branch-and-bound with AVX2 SIMD bitmask acceleration on the final dimension.
 */
inline Int128 evaluate_tensor_clean_cell_general(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    int d = cell.dim();
    if (d <= 2) return exact_kmp_sweep(boxes, d);

    std::vector<std::vector<std::pair<int64_t, int64_t>>> slabs_1(d);
    std::vector<std::vector<std::vector<Slab2>>> slabs_2(d, std::vector<std::vector<Slab2>>(d));

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) { disjoint = true; break; }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) noncov.push_back(i);
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
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

    std::vector<std::vector<std::pair<int64_t, int64_t>>> elementary(d);
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
                if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                else { coords.push_back(s.a2); coords.push_back(s.b2); }
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

    std::vector<size_t> V(d);
    for (int i = 0; i < d; ++i) {
        V[i] = elementary[i].size();
        if (V[i] == 0) return cell.volume();
    }

    std::vector<std::vector<int64_t>> D(d);
    std::vector<int64_t> sum_D(d, 0);
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
            sum_D[i] += D[i][r];
        }
        if (sum_D[i] == 0) return cell.volume();
    }

    // Fast-path: empty graph (tw = 0)
    int edge_count = 0;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) edge_count++;
        }
    }
    if (edge_count == 0) {
        Int128 comp_vol(1);
        for (int i = 0; i < d; ++i) {
            comp_vol = comp_vol * sum_D[i];
        }
        return cell.volume() - comp_vol;
    }

    int last = d - 1;
    SimdBitmask U_last(V[last], false);
    for (size_t r = 0; r < V[last]; ++r) {
        if (D[last][r] > 0) U_last.set(r);
    }

    std::vector<std::vector<SimdBitmask>> A_to_last(last);
    for (int u = 0; u < last; ++u) {
        A_to_last[u].assign(V[u], SimdBitmask(V[last], true));
        for (size_t ru = 0; ru < V[u]; ++ru) {
            int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
            for (size_t rlast = 0; rlast < V[last]; ++rlast) {
                int64_t llast = elementary[last][rlast].first, hlast = elementary[last][rlast].second;
                for (const auto& s : slabs_2[u][last]) {
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= llast && s.b2 >= hlast) {
                        A_to_last[u][ru].reset(rlast);
                        break;
                    }
                }
            }
        }
    }

    std::vector<std::vector<std::vector<uint8_t>>> A_table(last, std::vector<std::vector<uint8_t>>(last));
    for (int u = 0; u < last; ++u) {
        for (int v = u + 1; v < last; ++v) {
            A_table[u][v].assign(V[u] * V[v], 1);
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (const auto& s : slabs_2[u][v]) {
                        if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) {
                            A_table[u][v][ru * V[v] + rv] = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    Int128 complement_vol(0);
    std::vector<size_t> chosen(d, 0);

    auto dfs = [&](auto& self, int dim, Int128 current_weight, const SimdBitmask& current_mask) -> void {
        if (dim == last) {
            int64_t sum_last = 0;
            for (size_t w_idx = 0; w_idx < current_mask.words.size(); ++w_idx) {
                uint64_t word = current_mask.words[w_idx];
                while (word) {
                    int bit = count_trailing_zeros_u64(word);
                    size_t r = w_idx * 64 + bit;
                    if (r < V[last]) sum_last += D[last][r];
                    word = reset_lowest_set_bit_u64(word);
                }
            }
            if (sum_last > 0) {
                complement_vol += current_weight * sum_last;
            }
            return;
        }

        for (size_t r = 0; r < V[dim]; ++r) {
            if (D[dim][r] == 0) continue;
            bool ok = true;
            for (int prev = 0; prev < dim; ++prev) {
                if (!A_table[prev][dim][chosen[prev] * V[dim] + r]) {
                    ok = false;
                    break;
                }
            }
            if (!ok) continue;

            SimdBitmask next_mask = bitwise_and(current_mask, A_to_last[dim][r]);
            if (next_mask.is_zero()) continue;

            chosen[dim] = r;
            self(self, dim + 1, current_weight * D[dim][r], next_mask);
        }
    };

    dfs(dfs, 0, Int128(1), U_last);

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Unified Clean-Cell Tensor Network Solver Dispatcher
 */
/**
 * Exact d=4 Mixed (2,3)-Slab Hypergraph Tensor Network Contraction:
 * Complete hypergraph interaction network:
 * - 6 pairwise 2-slab edges: {0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3}
 * - 4 tripartite 3-slab hyperedges: {0,1,2}, {0,1,3}, {0,2,3}, {1,2,3}
 * Evaluated with AVX2 SIMD bitmask on coordinate 3.
 */
inline Int128 evaluate_tensor_hypergraph_d4(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 4;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[4];
    std::vector<Slab2> slabs_2[4][4];
    std::vector<Slab3> slabs_3[4][4][4];

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) { disjoint = true; break; }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) noncov.push_back(i);
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
        } else if (noncov.size() == 2) {
            int u = noncov[0], v = noncov[1];
            if (u > v) std::swap(u, v);
            int64_t l_u = std::max(cell.low[u], b.low[u]);
            int64_t h_u = std::min(cell.high[u], b.high[u]);
            int64_t l_v = std::max(cell.low[v], b.low[v]);
            int64_t h_v = std::min(cell.high[v], b.high[v]);
            slabs_2[u][v].push_back({u, v, l_u, h_u, l_v, h_v});
        } else if (noncov.size() == 3) {
            int u = noncov[0], v = noncov[1], w = noncov[2];
            if (u > v) std::swap(u, v);
            if (v > w) std::swap(v, w);
            if (u > v) std::swap(u, v);
            int64_t l_u = std::max(cell.low[u], b.low[u]);
            int64_t h_u = std::min(cell.high[u], b.high[u]);
            int64_t l_v = std::max(cell.low[v], b.low[v]);
            int64_t h_v = std::min(cell.high[v], b.high[v]);
            int64_t l_w = std::max(cell.low[w], b.low[w]);
            int64_t h_w = std::min(cell.high[w], b.high[w]);
            slabs_3[u][v][w].push_back({u, v, w, l_u, h_u, l_v, h_v, l_w, h_w});
        } else {
            return exact_kmp_sweep(boxes, d);
        }
    }

    std::vector<std::pair<int64_t, int64_t>> elementary[4];
    for (int i = 0; i < d; ++i) {
        std::vector<int64_t> coords;
        coords.push_back(cell.low[i]);
        coords.push_back(cell.high[i]);
        for (const auto& s : slabs_1[i]) {
            coords.push_back(s.first);
            coords.push_back(s.second);
        }
        for (int u = 0; u < d; ++u) {
            for (int v = u + 1; v < d; ++v) {
                for (const auto& s : slabs_2[u][v]) {
                    if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                    else if (i == v) { coords.push_back(s.a2); coords.push_back(s.b2); }
                }
            }
        }
        for (int u = 0; u < d; ++u) {
            for (int v = u + 1; v < d; ++v) {
                for (int w = v + 1; w < d; ++w) {
                    for (const auto& s : slabs_3[u][v][w]) {
                        if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                        else if (i == v) { coords.push_back(s.a2); coords.push_back(s.b2); }
                        else if (i == w) { coords.push_back(s.a3); coords.push_back(s.b3); }
                    }
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

    std::vector<int64_t> D[4];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
        }
    }

    int64_t sum_D[4] = {0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) sum_D[i] += D[i][r];
        if (sum_D[i] == 0) return cell.volume();
    }

    bool has_higher_slabs = false;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) has_higher_slabs = true;
            for (int w = v + 1; w < d; ++w) {
                if (!slabs_3[u][v][w].empty()) has_higher_slabs = true;
            }
        }
    }
    if (!has_higher_slabs) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        return cell.volume() - comp_vol;
    }

    // Build SIMD Bitmask for coordinate 3
    SimdBitmask U3(V[3], false);
    for (size_t r3 = 0; r3 < V[3]; ++r3) {
        if (D[3][r3] > 0) U3.set(r3);
    }

    std::vector<SimdBitmask> A_to_3[3];
    for (int u = 0; u < 3; ++u) {
        A_to_3[u].assign(V[u], SimdBitmask(V[3], true));
        for (size_t ru = 0; ru < V[u]; ++ru) {
            int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
            for (size_t r3 = 0; r3 < V[3]; ++r3) {
                int64_t l3 = elementary[3][r3].first, h3 = elementary[3][r3].second;
                for (const auto& s : slabs_2[u][3]) {
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= l3 && s.b2 >= h3) {
                        A_to_3[u][ru].reset(r3);
                        break;
                    }
                }
            }
        }
    }

    std::vector<SimdBitmask> H_to_3[3][3];
    for (int u = 0; u < 3; ++u) {
        for (int v = u + 1; v < 3; ++v) {
            H_to_3[u][v].assign(V[u] * V[v], SimdBitmask(V[3], true));
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (size_t r3 = 0; r3 < V[3]; ++r3) {
                        int64_t l3 = elementary[3][r3].first, h3 = elementary[3][r3].second;
                        for (const auto& s : slabs_3[u][v][3]) {
                            if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv && s.a3 <= l3 && s.b3 >= h3) {
                                H_to_3[u][v][ru * V[v] + rv].reset(r3);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<uint8_t> A_table[3][3];
    for (int u = 0; u < 3; ++u) {
        for (int v = u + 1; v < 3; ++v) {
            A_table[u][v].assign(V[u] * V[v], 1);
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (const auto& s : slabs_2[u][v]) {
                        if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) {
                            A_table[u][v][ru * V[v] + rv] = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    std::vector<uint8_t> H_012(V[0] * V[1] * V[2], 1);
    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t l0 = elementary[0][r0].first, h0 = elementary[0][r0].second;
        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t l1 = elementary[1][r1].first, h1 = elementary[1][r1].second;
            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                int64_t l2 = elementary[2][r2].first, h2 = elementary[2][r2].second;
                for (const auto& s : slabs_3[0][1][2]) {
                    if (s.a1 <= l0 && s.b1 >= h0 && s.a2 <= l1 && s.b2 >= h1 && s.a3 <= l2 && s.b3 >= h2) {
                        H_012[r0 * V[1] * V[2] + r1 * V[2] + r2] = 0;
                        break;
                    }
                }
            }
        }
    }

    Int128 complement_vol(0);

    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;
        SimdBitmask mask_03 = bitwise_and(U3, A_to_3[0][r0]);
        if (mask_03.is_zero()) continue;

        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t w1 = D[1][r1];
            if (w1 == 0 || !A_table[0][1][r0 * V[1] + r1]) continue;

            SimdBitmask mask_013 = bitwise_and(mask_03, A_to_3[1][r1]);
            mask_013 = bitwise_and(mask_013, H_to_3[0][1][r0 * V[1] + r1]);
            if (mask_013.is_zero()) continue;

            Int128 w01 = Int128(w0) * w1;

            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                int64_t w2 = D[2][r2];
                if (w2 == 0 || !A_table[0][2][r0 * V[2] + r2] || !A_table[1][2][r1 * V[2] + r2]) continue;
                if (!H_012[r0 * V[1] * V[2] + r1 * V[2] + r2]) continue;

                SimdBitmask mask_0123 = bitwise_and(mask_013, A_to_3[2][r2]);
                mask_0123 = bitwise_and(mask_0123, H_to_3[0][2][r0 * V[2] + r2]);
                mask_0123 = bitwise_and(mask_0123, H_to_3[1][2][r1 * V[2] + r2]);
                if (mask_0123.is_zero()) continue;

                Int128 w012 = w01 * w2;

                int64_t sum_3 = 0;
                for (size_t w_idx = 0; w_idx < mask_0123.words.size(); ++w_idx) {
                    uint64_t word = mask_0123.words[w_idx];
                    while (word) {
                        int bit = count_trailing_zeros_u64(word);
                        size_t r3 = w_idx * 64 + bit;
                        if (r3 < V[3]) sum_3 += D[3][r3];
                        word = reset_lowest_set_bit_u64(word);
                    }
                }
                if (sum_3 > 0) {
                    complement_vol += w012 * sum_3;
                }
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}

/**
 * Exact d=5 Mixed (2,3)-Slab Hypergraph Tensor Network Contraction:
 * Evaluated with AVX2 SIMD bitmask on coordinate 4.
 */
inline Int128 evaluate_tensor_hypergraph_d5(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    const int d = 5;
    std::vector<std::pair<int64_t, int64_t>> slabs_1[5];
    std::vector<Slab2> slabs_2[5][5];
    std::vector<Slab3> slabs_3[5][5][5];

    for (const auto& b : boxes) {
        if (!b.is_valid()) continue;
        std::vector<int> noncov;
        bool disjoint = false;
        for (int i = 0; i < d; ++i) {
            int64_t l = std::max(cell.low[i], b.low[i]);
            int64_t h = std::min(cell.high[i], b.high[i]);
            if (l >= h) { disjoint = true; break; }
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) noncov.push_back(i);
        }
        if (disjoint) continue;

        if (noncov.empty()) {
            return cell.volume();
        } else if (noncov.size() == 1) {
            int dim = noncov[0];
            slabs_1[dim].push_back({std::max(cell.low[dim], b.low[dim]), std::min(cell.high[dim], b.high[dim])});
        } else if (noncov.size() == 2) {
            int u = noncov[0], v = noncov[1];
            if (u > v) std::swap(u, v);
            int64_t l_u = std::max(cell.low[u], b.low[u]);
            int64_t h_u = std::min(cell.high[u], b.high[u]);
            int64_t l_v = std::max(cell.low[v], b.low[v]);
            int64_t h_v = std::min(cell.high[v], b.high[v]);
            slabs_2[u][v].push_back({u, v, l_u, h_u, l_v, h_v});
        } else if (noncov.size() == 3) {
            int u = noncov[0], v = noncov[1], w = noncov[2];
            if (u > v) std::swap(u, v);
            if (v > w) std::swap(v, w);
            if (u > v) std::swap(u, v);
            int64_t l_u = std::max(cell.low[u], b.low[u]);
            int64_t h_u = std::min(cell.high[u], b.high[u]);
            int64_t l_v = std::max(cell.low[v], b.low[v]);
            int64_t h_v = std::min(cell.high[v], b.high[v]);
            int64_t l_w = std::max(cell.low[w], b.low[w]);
            int64_t h_w = std::min(cell.high[w], b.high[w]);
            slabs_3[u][v][w].push_back({u, v, w, l_u, h_u, l_v, h_v, l_w, h_w});
        } else {
            return exact_kmp_sweep(boxes, d);
        }
    }

    std::vector<std::pair<int64_t, int64_t>> elementary[5];
    for (int i = 0; i < d; ++i) {
        std::vector<int64_t> coords;
        coords.push_back(cell.low[i]);
        coords.push_back(cell.high[i]);
        for (const auto& s : slabs_1[i]) {
            coords.push_back(s.first);
            coords.push_back(s.second);
        }
        for (int u = 0; u < d; ++u) {
            for (int v = u + 1; v < d; ++v) {
                for (const auto& s : slabs_2[u][v]) {
                    if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                    else if (i == v) { coords.push_back(s.a2); coords.push_back(s.b2); }
                }
            }
        }
        for (int u = 0; u < d; ++u) {
            for (int v = u + 1; v < d; ++v) {
                for (int w = v + 1; w < d; ++w) {
                    for (const auto& s : slabs_3[u][v][w]) {
                        if (i == u) { coords.push_back(s.a1); coords.push_back(s.b1); }
                        else if (i == v) { coords.push_back(s.a2); coords.push_back(s.b2); }
                        else if (i == w) { coords.push_back(s.a3); coords.push_back(s.b3); }
                    }
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

    size_t V[5];
    for (int i = 0; i < d; ++i) {
        V[i] = elementary[i].size();
        if (V[i] == 0) return cell.volume();
    }

    std::vector<int64_t> D[5];
    for (int i = 0; i < d; ++i) {
        D[i].resize(V[i], 0);
        for (size_t r = 0; r < V[i]; ++r) {
            int64_t l = elementary[i][r].first, h = elementary[i][r].second;
            bool cov = false;
            for (const auto& s : slabs_1[i]) {
                if (s.first <= l && s.second >= h) { cov = true; break; }
            }
            if (!cov) D[i][r] = h - l;
        }
    }

    int64_t sum_D[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < d; ++i) {
        for (size_t r = 0; r < V[i]; ++r) sum_D[i] += D[i][r];
        if (sum_D[i] == 0) return cell.volume();
    }

    bool has_higher_slabs = false;
    for (int u = 0; u < d; ++u) {
        for (int v = u + 1; v < d; ++v) {
            if (!slabs_2[u][v].empty()) has_higher_slabs = true;
            for (int w = v + 1; w < d; ++w) {
                if (!slabs_3[u][v][w].empty()) has_higher_slabs = true;
            }
        }
    }
    if (!has_higher_slabs) {
        Int128 comp_vol = Int128(sum_D[0]) * sum_D[1];
        comp_vol = comp_vol * sum_D[2];
        comp_vol = comp_vol * sum_D[3];
        comp_vol = comp_vol * sum_D[4];
        return cell.volume() - comp_vol;
    }

    // Build SIMD Bitmask for coordinate 4
    SimdBitmask U4(V[4], false);
    for (size_t r4 = 0; r4 < V[4]; ++r4) {
        if (D[4][r4] > 0) U4.set(r4);
    }

    std::vector<SimdBitmask> A_to_4[4];
    for (int u = 0; u < 4; ++u) {
        A_to_4[u].assign(V[u], SimdBitmask(V[4], true));
        for (size_t ru = 0; ru < V[u]; ++ru) {
            int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
            for (size_t r4 = 0; r4 < V[4]; ++r4) {
                int64_t l4 = elementary[4][r4].first, h4 = elementary[4][r4].second;
                for (const auto& s : slabs_2[u][4]) {
                    if (s.a1 <= lu && s.b1 >= hu && s.a2 <= l4 && s.b2 >= h4) {
                        A_to_4[u][ru].reset(r4);
                        break;
                    }
                }
            }
        }
    }

    std::vector<SimdBitmask> H_to_4[4][4];
    for (int u = 0; u < 4; ++u) {
        for (int v = u + 1; v < 4; ++v) {
            H_to_4[u][v].assign(V[u] * V[v], SimdBitmask(V[4], true));
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (size_t r4 = 0; r4 < V[4]; ++r4) {
                        int64_t l4 = elementary[4][r4].first, h4 = elementary[4][r4].second;
                        for (const auto& s : slabs_3[u][v][4]) {
                            if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv && s.a3 <= l4 && s.b3 >= h4) {
                                H_to_4[u][v][ru * V[v] + rv].reset(r4);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<uint8_t> A_table[4][4];
    for (int u = 0; u < 4; ++u) {
        for (int v = u + 1; v < 4; ++v) {
            A_table[u][v].assign(V[u] * V[v], 1);
            for (size_t ru = 0; ru < V[u]; ++ru) {
                int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                for (size_t rv = 0; rv < V[v]; ++rv) {
                    int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                    for (const auto& s : slabs_2[u][v]) {
                        if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv) {
                            A_table[u][v][ru * V[v] + rv] = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    std::vector<uint8_t> H_table[4][4][4];
    for (int u = 0; u < 4; ++u) {
        for (int v = u + 1; v < 4; ++v) {
            for (int w = v + 1; w < 4; ++w) {
                H_table[u][v][w].assign(V[u] * V[v] * V[w], 1);
                for (size_t ru = 0; ru < V[u]; ++ru) {
                    int64_t lu = elementary[u][ru].first, hu = elementary[u][ru].second;
                    for (size_t rv = 0; rv < V[v]; ++rv) {
                        int64_t lv = elementary[v][rv].first, hv = elementary[v][rv].second;
                        for (size_t rw = 0; rw < V[w]; ++rw) {
                            int64_t lw = elementary[w][rw].first, hw = elementary[w][rw].second;
                            for (const auto& s : slabs_3[u][v][w]) {
                                if (s.a1 <= lu && s.b1 >= hu && s.a2 <= lv && s.b2 >= hv && s.a3 <= lw && s.b3 >= hw) {
                                    H_table[u][v][w][ru * V[v] * V[w] + rv * V[w] + rw] = 0;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Int128 complement_vol(0);

    for (size_t r0 = 0; r0 < V[0]; ++r0) {
        int64_t w0 = D[0][r0];
        if (w0 == 0) continue;
        SimdBitmask mask_04 = bitwise_and(U4, A_to_4[0][r0]);
        if (mask_04.is_zero()) continue;

        for (size_t r1 = 0; r1 < V[1]; ++r1) {
            int64_t w1 = D[1][r1];
            if (w1 == 0 || !A_table[0][1][r0 * V[1] + r1]) continue;

            SimdBitmask mask_014 = bitwise_and(mask_04, A_to_4[1][r1]);
            mask_014 = bitwise_and(mask_014, H_to_4[0][1][r0 * V[1] + r1]);
            if (mask_014.is_zero()) continue;

            Int128 w01 = Int128(w0) * w1;

            for (size_t r2 = 0; r2 < V[2]; ++r2) {
                int64_t w2 = D[2][r2];
                if (w2 == 0 || !A_table[0][2][r0 * V[2] + r2] || !A_table[1][2][r1 * V[2] + r2]) continue;
                if (!H_table[0][1][2][r0 * V[1] * V[2] + r1 * V[2] + r2]) continue;

                SimdBitmask mask_0124 = bitwise_and(mask_014, A_to_4[2][r2]);
                mask_0124 = bitwise_and(mask_0124, H_to_4[0][2][r0 * V[2] + r2]);
                mask_0124 = bitwise_and(mask_0124, H_to_4[1][2][r1 * V[2] + r2]);
                if (mask_0124.is_zero()) continue;

                Int128 w012 = w01 * w2;

                for (size_t r3 = 0; r3 < V[3]; ++r3) {
                    int64_t w3 = D[3][r3];
                    if (w3 == 0 || !A_table[0][3][r0 * V[3] + r3] || !A_table[1][3][r1 * V[3] + r3] || !A_table[2][3][r2 * V[3] + r3]) continue;
                    if (!H_table[0][1][3][r0 * V[1] * V[3] + r1 * V[3] + r3] ||
                        !H_table[0][2][3][r0 * V[2] * V[3] + r2 * V[3] + r3] ||
                        !H_table[1][2][3][r1 * V[2] * V[3] + r2 * V[3] + r3]) continue;

                    SimdBitmask mask_final = bitwise_and(mask_0124, A_to_4[3][r3]);
                    mask_final = bitwise_and(mask_final, H_to_4[0][3][r0 * V[3] + r3]);
                    mask_final = bitwise_and(mask_final, H_to_4[1][3][r1 * V[3] + r3]);
                    mask_final = bitwise_and(mask_final, H_to_4[2][3][r2 * V[3] + r3]);
                    if (mask_final.is_zero()) continue;

                    Int128 w0123 = w012 * w3;

                    int64_t sum_4 = 0;
                    for (size_t w_idx = 0; w_idx < mask_final.words.size(); ++w_idx) {
                        uint64_t word = mask_final.words[w_idx];
                        while (word) {
                            int bit = count_trailing_zeros_u64(word);
                            size_t r4 = w_idx * 64 + bit;
                            if (r4 < V[4]) sum_4 += D[4][r4];
                            word = reset_lowest_set_bit_u64(word);
                        }
                    }
                    if (sum_4 > 0) {
                        complement_vol += w0123 * sum_4;
                    }
                }
            }
        }
    }

    Int128 total_vol = cell.volume();
    return total_vol - complement_vol;
}


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
        if (is_clean_cell(cell, boxes)) return evaluate_tensor_clean_cell_d4(cell, boxes);
        if (is_hypergraph_clean_cell(cell, boxes)) return evaluate_tensor_hypergraph_d4(cell, boxes);
        return evaluate_tensor_clean_cell_d4(cell, boxes);
    } else if (d == 5) {
        if (is_clean_cell(cell, boxes)) return evaluate_tensor_clean_cell_d5(cell, boxes);
        if (is_hypergraph_clean_cell(cell, boxes)) return evaluate_tensor_hypergraph_d5(cell, boxes);
        return evaluate_tensor_clean_cell_d5(cell, boxes);
    } else if (d == 6) {
        return evaluate_tensor_clean_cell_d6(cell, boxes);
    } else if (d == 7) {
        return evaluate_tensor_clean_cell_d7(cell, boxes);
    } else if (is_clean_cell(cell, boxes)) {
        return evaluate_tensor_clean_cell_general(cell, boxes);
    } else {
        return exact_kmp_sweep(boxes, d);
    }
}

} // namespace kmp
