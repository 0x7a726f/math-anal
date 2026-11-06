#pragma once
#include "types.hpp"
#include <random>
#include <vector>
#include <cassert>

namespace kmp {

// Simple deterministic 64-bit LCG
struct FastRng {
    uint64_t state;
    FastRng(uint64_t seed = 42) : state(seed ? seed : 42) {}
    uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
    int64_t next_range(int64_t low, int64_t high) {
        if (high <= low) return low;
        return low + (int64_t)(next() % (uint64_t)(high - low + 1));
    }
};

// 1. Orthogonal Trellis Weaves
// Generates n 2-slab boxes in R^d within bounding cell [0, M]^d.
// Pairs of coordinates (i, j) are chosen systematically to create a dense
// cross-weave of orthogonal ridges where any single cut hyperplane splits
// a large fraction of active (d-2)-faces.
inline std::vector<Box> generate_trellis_weave(int n, int d, uint64_t seed = 42) {
    FastRng rng(seed);
    const int64_t M = 1000000; // [0, 10^6]
    std::vector<Box> boxes;
    boxes.reserve(n);

    // Enumerate all C(d, 2) dimension pairs
    std::vector<std::pair<int, int>> pairs;
    for (int i = 0; i < d; ++i) {
        for (int j = i + 1; j < d; ++j) {
            pairs.push_back({i, j});
        }
    }
    assert(!pairs.empty());

    for (int k = 0; k < n; ++k) {
        auto p = pairs[k % pairs.size()];
        int dim1 = p.first;
        int dim2 = p.second;

        Box b;
        b.low.assign(d, 0);
        b.high.assign(d, M);

        // Constrain dim1 and dim2 to small/medium sub-intervals
        int64_t u1 = rng.next_range(0, M - 50000);
        int64_t len1 = rng.next_range(20000, 100000);
        b.low[dim1] = u1;
        b.high[dim1] = std::min(M, u1 + len1);

        int64_t u2 = rng.next_range(0, M - 50000);
        int64_t len2 = rng.next_range(20000, 100000);
        b.low[dim2] = u2;
        b.high[dim2] = std::min(M, u2 + len2);

        boxes.push_back(b);
    }
    return boxes;
}

// 2. Hyperclique Complement Gadgets
// Constructed from a simulated 3-uniform / d-partite hypergraph with V vertices per part.
// Coordinates in dimension i represent vertices {0, ..., V-1} scaled to integer intervals.
// Boxes forbid non-edges or simulate consistency constraints, concentrating (d-2)-faces
// into dense combinatorial clusters.
inline std::vector<Box> generate_hyperclique_gadget(int n, int d, uint64_t seed = 12345) {
    FastRng rng(seed);
    const int64_t M = 1000000;
    std::vector<Box> boxes;
    boxes.reserve(n);

    int64_t V = 8; // 8 discrete vertex slots per coordinate
    int64_t slot_w = M / V;

    for (int k = 0; k < n; ++k) {
        Box b;
        b.low.assign(d, 0);
        b.high.assign(d, M);

        // Pick 2 or 3 active dimensions that are constrained to discrete vertex slots
        int num_constrained = 2 + (rng.next() % (d > 3 ? 2 : 1));
        std::vector<int> perm(d);
        for (int i = 0; i < d; ++i) perm[i] = i;
        for (int i = 0; i < d; ++i) {
            int j = i + (rng.next() % (d - i));
            std::swap(perm[i], perm[j]);
        }

        for (int c = 0; c < num_constrained; ++c) {
            int dim = perm[c];
            int64_t slot = rng.next_range(0, V - 2);
            int64_t span = rng.next_range(1, 2);
            b.low[dim] = slot * slot_w;
            b.high[dim] = std::min(M, (slot + span) * slot_w);
        }

        boxes.push_back(b);
    }
    return boxes;
}

// 3. Small Verification Generator
// Generates diverse small instances for strict 100% ground-truth verification
inline std::vector<Box> generate_small_test(int n, int d, uint64_t seed) {
    FastRng rng(seed);
    const int64_t M = 1000;
    std::vector<Box> boxes;
    boxes.reserve(n);
    for (int k = 0; k < n; ++k) {
        Box b;
        b.low.resize(d);
        b.high.resize(d);
        for (int i = 0; i < d; ++i) {
            int64_t u = rng.next_range(0, M - 10);
            int64_t v = rng.next_range(u + 1, M);
            b.low[i] = u;
            b.high[i] = v;
        }
        boxes.push_back(b);
    }
    return boxes;
}


// 4. Epsilon-Jitter Perturbation Generator (Directive V2 Mode B)
// Adds pseudo-random perturbation epsilon in [-10^-9, 10^-9] (scaled to integer grid domain)
// to all box coordinates to break lattice symmetries, forcing V = Theta(n) distinct coordinates
// per axis and verifying genuine spatial bisection until subcell valence V_sub <= V_cutoff.
inline std::vector<Box> apply_epsilon_jitter(const std::vector<Box>& boxes, int d, uint64_t seed = 99991) {
    FastRng rng(seed);
    const int64_t M = 1000000;
    std::vector<Box> jittered = boxes;
    for (size_t k = 0; k < jittered.size(); ++k) {
        for (int i = 0; i < d; ++i) {
            if (jittered[k].low[i] > 0) {
                int64_t eps = rng.next_range(-250, 250);
                jittered[k].low[i] = std::max((int64_t)1, jittered[k].low[i] + eps);
            }
            if (jittered[k].high[i] < M) {
                int64_t eps = rng.next_range(-250, 250);
                jittered[k].high[i] = std::min(M - 1, std::max(jittered[k].low[i] + 1, jittered[k].high[i] + eps));
            }
        }
    }
    return jittered;
}

} // namespace kmp

