#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

// C++ Implementation of Clean-Cell Matrix Trace for d=3
inline Int128 evaluate_clean_cell_d3_trace(
    const Box& cell,
    const std::vector<Box>& boxes
) {
    int d = 3;
    // Step 1: Identify 1-slabs and 2-slabs
    std::vector<std::pair<int64_t, int64_t>> slabs_1[3];
    struct Slab2 { int64_t a1, b1, a2, b2; };
    std::vector<Slab2> slabs_2_01; // pair (0, 1)
    std::vector<Slab2> slabs_2_12; // pair (1, 2)
    std::vector<Slab2> slabs_2_02; // pair (0, 2)

    for (const auto& b : boxes) {
        std::vector<int> noncov;
        for (int i = 0; i < d; ++i) {
            if (b.low[i] > cell.low[i] || b.high[i] < cell.high[i]) {
                noncov.push_back(i);
            }
        }
        if (noncov.empty()) {
            // 0-slab: entire cell covered
            return cell.volume();
        } else if (noncov.size() == 1) {
            slabs_1[noncov[0]].push_back({b.low[noncov[0]], b.high[noncov[0]]});
        } else if (noncov.size() == 2) {
            int u = noncov[0], v = noncov[1];
            if (u > v) std::swap(u, v);
            if (u == 0 && v == 1) {
                slabs_2_01.push_back({b.low[0], b.high[0], b.low[1], b.high[1]});
            } else if (u == 1 && v == 2) {
                slabs_2_12.push_back({b.low[1], b.high[1], b.low[2], b.high[2]});
            } else {
                slabs_2_02.push_back({b.low[0], b.high[0], b.low[2], b.high[2]});
            }
        } else {
            // Not clean: has 3-slab, fall back to exact sweep
            return exact_kmp_sweep(boxes, d);
        }
    }

    // Step 2: Build elementary intervals per coordinate
    std::vector<std::pair<int64_t, int64_t>> elementary[3];
    for (int i = 0; i < d; ++i) {
        std::vector<int64_t> coords;
        coords.push_back(cell.low[i]);
        coords.push_back(cell.high[i]);
        for (const auto& s : slabs_1[i]) {
            coords.push_back(std::max(cell.low[i], std::min(cell.high[i], s.first)));
            coords.push_back(std::max(cell.low[i], std::min(cell.high[i], s.second)));
        }
        // Add 2-slab coordinates
        if (i == 0) {
            for (const auto& s : slabs_2_01) {
                coords.push_back(std::max(cell.low[0], std::min(cell.high[0], s.a1)));
                coords.push_back(std::max(cell.low[0], std::min(cell.high[0], s.b1)));
            }
            for (const auto& s : slabs_2_02) {
                coords.push_back(std::max(cell.low[0], std::min(cell.high[0], s.a1)));
                coords.push_back(std::max(cell.low[0], std::min(cell.high[0], s.b1)));
            }
        } else if (i == 1) {
            for (const auto& s : slabs_2_01) {
                coords.push_back(std::max(cell.low[1], std::min(cell.high[1], s.a2)));
                coords.push_back(std::max(cell.low[1], std::min(cell.high[1], s.b2)));
            }
            for (const auto& s : slabs_2_12) {
                coords.push_back(std::max(cell.low[1], std::min(cell.high[1], s.a1)));
                coords.push_back(std::max(cell.low[1], std::min(cell.high[1], s.b1)));
            }
        } else { // i == 2
            for (const auto& s : slabs_2_12) {
                coords.push_back(std::max(cell.low[2], std::min(cell.high[2], s.a2)));
                coords.push_back(std::max(cell.low[2], std::min(cell.high[2], s.b2)));
            }
            for (const auto& s : slabs_2_02) {
                coords.push_back(std::max(cell.low[2], std::min(cell.high[2], s.a2)));
                coords.push_back(std::max(cell.low[2], std::min(cell.high[2], s.b2)));
            }
        }
        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());
        for (size_t r = 0; r + 1 < coords.size(); ++r) {
            if (coords[r+1] > coords[r]) {
                elementary[i].push_back({coords[r], coords[r+1]});
            }
        }
    }

    // Step 3: Compute diagonal weights D_i
    std::vector<int64_t> D[3];
    for (int i = 0; i < d; ++i) {
        D[i].resize(elementary[i].size(), 0);
        for (size_t r = 0; r < elementary[i].size(); ++r) {
            int64_t l = elementary[i][r].first;
            int64_t h = elementary[i][r].second;
            // Check if covered by any 1-slab in dimension i
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

    size_t V0 = elementary[0].size();
    size_t V1 = elementary[1].size();
    size_t V2 = elementary[2].size();

    if (V0 == 0 || V1 == 0 || V2 == 0) {
        return cell.volume();
    }

    // Step 4: Build binary adjacency matrices A01, A12, A20
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

    // Step 5: Compute Tr(D0 A01 D1 A12 D2 A20) with exact Int128
    // M1 = D0 * A01 * D1 : size V0 x V1
    // M2 = M1 * A12      : size V0 x V2
    // M3 = M2 * D2       : size V0 x V2
    // Trace = sum_{r0, r2} M3[r0, r2] * A20[r2, r0]
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

int main() {
    std::cout << "Testing C++ Clean-Cell Matrix Trace Engine against ground truth sweep..." << std::endl;

    for (int seed = 100; seed < 150; ++seed) {
        auto boxes = generate_trellis_weave(40, 3, seed);
        Box root_cell;
        root_cell.low = {0, 0, 0};
        root_cell.high = {1000000, 1000000, 1000000};

        Int128 ref_vol = exact_kmp_sweep(boxes, 3);
        Int128 trace_vol = evaluate_clean_cell_d3_trace(root_cell, boxes);

        if (ref_vol != trace_vol) {
            std::cerr << "FAIL on seed " << seed << ": ref=" << int128_to_string(ref_vol)
                      << " trace=" << int128_to_string(trace_vol) << std::endl;
            return 1;
        }
    }

    std::cout << "SUCCESS: 50 Trellis Weave instances 100% matched between C++ Matrix Trace and Ground Truth!" << std::endl;
    return 0;
}
