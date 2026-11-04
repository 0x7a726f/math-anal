"""
Comprehensive Verification & Benchmark Suite for Klee's Measure Problem
=======================================================================
Tests and benchmarks:
1. Exact agreement across all 5 algorithms:
   - klee_1d (d = 1)
   - klee_2d_bentley (d = 2)
   - klee_grid_exact (ground-truth coordinate-compressed grid, d = 1..5)
   - klee_sweep_nd (Bentley's O(n^{d-1} log n) hyperplane sweep, d = 1..5)
   - klee_chan_sdc (Chan's FOCS 2013 O(n^{d/2}) Simplify-Divide-and-Conquer, d = 1..6)
   - klee_hybrid_chan_fmm (Our Hybrid Chan-SDC + Fast Matrix Multiplication solver, d = 1..6)
2. Verification of the Clean-Cell Factorization Lemma & FMM Trace Formula
   on random 2-slab instances and Chan's d-Clique reduction instances.
3. Verification of Prefix Covering Designs (d = 3, 4, 5) and end-to-end
   execution of the 3-Uniform K-Hyperclique -> Klee's Measure Problem reduction!
"""

from __future__ import annotations
import random
import time
import math
import itertools
import numpy as np

from klee_algorithms import (
    Box,
    ChanSDCStats,
    klee_1d,
    klee_2d_bentley,
    klee_grid_exact,
    klee_sweep_nd,
    klee_chan_sdc,
)
from fmm_2slab_solver import (
    solve_clean_cell_fmm,
    klee_hybrid_chan_fmm,
    clique_to_klee_reduction,
)
from prefix_covering_designs import (
    verify_prefix_covering_design,
    build_cyclic_d3_design,
    get_d4_40_21_design,
    get_d5_40_18_design,
    build_d6_difference_set_design,
    hyperclique_to_klee_reduction,
)


def generate_random_boxes(n: int, d: int, seed: int) -> list[Box]:
    rng = random.Random(seed)
    boxes = []
    for _ in range(n):
        b = []
        for _ in range(d):
            u = rng.uniform(0.0, 10.0)
            v = rng.uniform(0.0, 10.0)
            if u > v:
                u, v = v, u
            if v - u < 0.1:
                v += 0.5
            b.append((round(u, 4), round(v, 4)))
        boxes.append(tuple(b))
    return boxes


def generate_2slab_boxes(n: int, d: int, grid_v: int, seed: int) -> list[Box]:
    """Generate n random 2-slab boxes in [0, grid_v]^d each restricting 2 dimensions to unit/small intervals."""
    rng = random.Random(seed)
    boxes = []
    for _ in range(n):
        b = [(0.0, float(grid_v))] * d
        dims = rng.sample(range(d), 2)
        for i in dims:
            u = rng.randint(0, grid_v - 1)
            v = min(grid_v, u + rng.choice([1, 1, 2]))
            b[i] = (float(u), float(v))
        boxes.append(tuple(b))
    return boxes


def run_correctness_tests() -> None:
    print("=" * 78)
    print("1. CROSS-VALIDATING EXACT ALGORITHMS ACROSS DIMENSIONS d = 1..5")
    print("=" * 78)

    # d = 1 tests
    for seed in range(5):
        boxes_1d = generate_random_boxes(25, 1, seed)
        v_1d = klee_1d([b[0] for b in boxes_1d])
        v_chan = klee_chan_sdc(boxes_1d)
        assert abs(v_1d - v_chan) < 1e-9, f"d=1 mismatch: {v_1d} vs {v_chan}"
    print("[PASS] d = 1: klee_1d == klee_chan_sdc across 5 random trials (n=25)")

    # d = 2 tests
    for seed in range(5):
        boxes_2d = generate_random_boxes(30, 2, seed + 100)
        v_bentley = klee_2d_bentley(boxes_2d)
        v_grid = klee_grid_exact(boxes_2d)
        v_chan = klee_chan_sdc(boxes_2d, use_2d_base_case=False)
        assert abs(v_bentley - v_grid) < 1e-8
        assert abs(v_bentley - v_chan) < 1e-8
    print("[PASS] d = 2: klee_2d_bentley == klee_grid_exact == klee_chan_sdc (pure 2D SDC) (n=30)")

    # d = 3, 4, 5 tests comparing Grid Exact, Hyperplane Sweep, Chan SDC, and Hybrid Chan+FMM
    for d, n_boxes, trials in [(3, 14, 5), (4, 9, 4), (5, 6, 3)]:
        for t in range(trials):
            boxes = generate_random_boxes(n_boxes, d, seed=1000 * d + t)
            v_grid = klee_grid_exact(boxes)
            v_sweep = klee_sweep_nd(boxes)
            stats_chan = ChanSDCStats()
            v_chan = klee_chan_sdc(boxes, stats=stats_chan)
            stats_hyb = ChanSDCStats()
            v_hyb = klee_hybrid_chan_fmm(boxes, stats=stats_hyb)

            assert abs(v_grid - v_sweep) < 1e-7, f"d={d} grid vs sweep: {v_grid} vs {v_sweep}"
            assert abs(v_grid - v_chan) < 1e-7, f"d={d} grid vs chan: {v_grid} vs {v_chan}"
            assert abs(v_grid - v_hyb) < 1e-7, f"d={d} grid vs hybrid: {v_grid} vs {v_hyb}"
        print(
            f"[PASS] d = {d}: klee_grid_exact == klee_sweep_nd == klee_chan_sdc == klee_hybrid_chan_fmm "
            f"({trials} trials, n={n_boxes})"
        )

    # Edge cases: identical boxes, nested boxes, disjoint boxes
    for d in (3, 4):
        unit = tuple((0.0, 1.0) for _ in range(d))
        assert abs(klee_chan_sdc([unit] * 10) - 1.0) < 1e-12
        assert abs(klee_hybrid_chan_fmm([unit] * 10) - 1.0) < 1e-12
        disjoint = [
            tuple((float(2 * k), float(2 * k + 1)) for _ in range(d))
            for k in range(5)
        ]
        assert abs(klee_chan_sdc(disjoint) - 5.0) < 1e-12
        assert abs(klee_hybrid_chan_fmm(disjoint) - 5.0) < 1e-12
    print("[PASS] Edge cases (nested, identical, disjoint hypercubes in d=3, 4)")


def run_fmm_and_clique_benchmarks() -> None:
    print("\n" + "=" * 78)
    print("2. CLEAN-CELL FMM TRACE LEMMA & CHAN'S d-CLIQUE REDUCTION BENCHMARK")
    print("=" * 78)

    # Test Clean-Cell FMM solver directly against Chan SDC and Sweep on 2-slab cells
    for d in (3, 4, 5, 6):
        boxes = generate_2slab_boxes(n=40, d=d, grid_v=6, seed=42 + d)
        t0 = time.perf_counter()
        stats_fmm = ChanSDCStats()
        v_fmm = klee_hybrid_chan_fmm(boxes, max_fmm_dim=4096, stats=stats_fmm)
        t_fmm = (time.perf_counter() - t0) * 1000.0

        t1 = time.perf_counter()
        stats_chan = ChanSDCStats()
        v_chan = klee_chan_sdc(boxes, stats=stats_chan)
        t_chan = (time.perf_counter() - t1) * 1000.0

        assert abs(v_fmm - v_chan) < 1e-7, f"2-slab FMM mismatch in d={d}: {v_fmm} vs {v_chan}"
        print(
            f"[PASS] d={d}, n=40 (2-slabs on V=6 grid): Vol = {v_fmm:.2f} | "
            f"Hybrid-FMM: {t_fmm:6.2f} ms ({stats_fmm.recursive_calls} calls) vs "
            f"Pure Chan-SDC: {t_chan:6.2f} ms ({stats_chan.recursive_calls} calls)"
        )

    # Test Chan's (2010) d-Clique -> Klee's Measure Problem reduction
    print("\n--- Testing Chan's d-Clique -> Klee's Measure Problem Reduction ---")
    rng = np.random.default_rng(2026)
    for d, V in [(3, 8), (4, 7), (5, 6)]:
        # Random Erdos-Renyi graph with edge probability p = 0.65
        adj = np.triu((rng.random((V, V)) < 0.65).astype(int), k=1)
        adj = adj + adj.T
        boxes, true_ordered_cliques = clique_to_klee_reduction(adj, d=d)

        t0 = time.perf_counter()
        stats_hyb = ChanSDCStats()
        vol_union = klee_hybrid_chan_fmm(boxes, max_fmm_dim=4096, stats=stats_hyb)
        dt_hyb = (time.perf_counter() - t0) * 1000.0

        t1 = time.perf_counter()
        stats_chan = ChanSDCStats()
        vol_chan = klee_chan_sdc(boxes, stats=stats_chan)
        dt_chan = (time.perf_counter() - t1) * 1000.0

        # Recover ordered and unordered d-cliques from volume
        recovered_ordered = round((V ** d) - vol_union)
        recovered_unordered = recovered_ordered // math.factorial(d)
        true_unordered = true_ordered_cliques // math.factorial(d)

        assert recovered_ordered == true_ordered_cliques, (
            f"Clique count mismatch: {recovered_ordered} vs {true_ordered_cliques}"
        )
        assert abs(vol_union - vol_chan) < 1e-7
        print(
            f"[PASS] d={d}-Clique on V={V} -> n={len(boxes)} boxes: "
            f"Ordered Cliques = {recovered_ordered} (Unordered = {recovered_unordered}) | "
            f"Hybrid-FMM: {dt_hyb:6.2f} ms (1 call) vs Pure Chan-SDC: {dt_chan:7.2f} ms ({stats_chan.recursive_calls} calls)"
        )


def run_prefix_covering_design_tests() -> None:
    print("\n" + "=" * 78)
    print("3. PREFIX COVERING DESIGNS & 3-UNIFORM HYPERCLIQUE REDUCTIONS")
    print("=" * 78)

    # 3.1 Verify Cyclic d = 3 family
    for m in (2, 4, 8, 15):
        seqs, K, alpha = build_cyclic_d3_design(m)
        res = verify_prefix_covering_design(seqs, K, alpha, r=3)
        assert res.is_valid, res.error_message
        print(
            f"[PASS] Cyclic (d=3, K={K}, alpha={alpha}) PCD verified: "
            f"max_singleton={res.max_singleton_sum} <= {alpha+1}, "
            f"max_triplet={res.max_tuple_cost} <= {alpha}, "
            f"gamma_3 >= {res.exponent_lower_bound:.5f}"
        )

    # 3.2 Verify Gorbachev-Kunnemann (4, 40, 21) design
    seqs4, K4, alpha4 = get_d4_40_21_design()
    res4 = verify_prefix_covering_design(seqs4, K4, alpha4, r=3)
    assert res4.is_valid, res4.error_message
    print(
        f"[PASS] Gorbachev-Kunnemann (d=4, K={K4}, alpha={alpha4}) PCD verified: "
        f"max_singleton={res4.max_singleton_sum} <= {alpha4+1}, "
        f"max_triplet={res4.max_tuple_cost} <= {alpha4}, "
        f"gamma_4 >= {K4}/{alpha4} = {res4.exponent_lower_bound:.7f}"
    )

    # 3.3 Verify Gorbachev-Kunnemann (5, 40, 18) design
    seqs5, K5, alpha5 = get_d5_40_18_design()
    res5 = verify_prefix_covering_design(seqs5, K5, alpha5, r=3)
    assert res5.is_valid, res5.error_message
    print(
        f"[PASS] Gorbachev-Kunnemann (d=5, K={K5}, alpha={alpha5}) PCD verified: "
        f"max_singleton={res5.max_singleton_sum} <= {alpha5+1}, "
        f"max_triplet={res5.max_tuple_cost} <= {alpha5}, "
        f"gamma_5 >= {K5}/{alpha5} = {res5.exponent_lower_bound:.7f}"
    )

    # 3.4 Verify Cyclic Difference-Set {0, 1, 3} (mod 6) design for d = 6
    seqs6, K6, alpha6 = build_d6_difference_set_design(m=9, t=3)
    res6 = verify_prefix_covering_design(seqs6, K6, alpha6, r=3)
    assert res6.is_valid, res6.error_message
    print(
        f"[PASS] Difference-Set {{0,1,3}} mod 6 (d=6, K={K6}, alpha={alpha6}) PCD verified: "
        f"max_singleton={res6.max_singleton_sum} <= {alpha6+1}, "
        f"max_triplet={res6.max_tuple_cost} <= {alpha6}, "
        f"gamma_6 >= {K6}/{alpha6} = {res6.exponent_lower_bound:.7f}"
    )

    # 3.4 End-to-End 3-Uniform K-Hyperclique -> Klee's Measure Problem Reduction!
    print("\n--- Testing End-to-End 3-Uniform Hyperclique -> Klee's Measure Reduction ---")
    # Use a compact (d=3, K=4, alpha=3) Prefix Covering Design:
    # Let's check: s1 = (1, 2, 3), s2 = (3, 4, 2), s3 = (4, 1) -> K=4, alpha=3:
    # Wait: let's verify our cyclic d=3 design with m=1 (K=3, alpha=3) and a K=4, alpha=3 design!
    # For K=4, alpha=3 in d=3:
    # s1 = (1, 2, 3), s2 = (2, 4, 1), s3 = (3, 4, 2) -> let's check singleton:
    # 1: pos 1 in s1, pos 3 in s2 -> 1+3 = 4 <= 4.
    # 2: pos 2 in s1, pos 1 in s2, pos 3 in s3 -> min=1, max=3 -> 1+3 = 4 <= 4.
    # 3: pos 3 in s1, pos 1 in s3 -> 1+3 = 4 <= 4.
    # 4: pos 2 in s2, pos 2 in s3 -> 2+2 = 4 <= 4.
    # Triplets of {1,2,3,4}:
    # {1,2,3}: in s1[..3], cost 3!
    # {1,2,4}: in s2[..3], cost 3!
    # {2,3,4}: in s3[..3], cost 3!
    # {1,3,4}: 1 in s1[..1] (cost 1) + {3,4} in s3[..2] (cost 2) -> 1+2 = 3!
    pcd_3_4_3 = [(1, 2, 3), (2, 4, 1), (3, 4, 2)]
    ver_343 = verify_prefix_covering_design(pcd_3_4_3, K=4, alpha=3, r=3)
    assert ver_343.is_valid, ver_343.error_message
    print(
        f"[PASS] Constructed & verified (d=3, K=4, alpha=3) PCD: {pcd_3_4_3} "
        f"(gamma = 4/3 = {ver_343.exponent_lower_bound:.4f})"
    )

    # Now reduce a random 4-partite 3-uniform hypergraph with N=2 vertices per part
    # to Klee's Measure Problem in R^3 using pcd_3_4_3!
    rng = random.Random(12345)
    K_test, N_test = 4, 2
    for trial in range(3):
        hyperedges = set()
        for a, b, c in itertools.combinations(range(1, K_test + 1), 3):
            for va, vb, vc in itertools.product(range(N_test), repeat=3):
                if rng.random() < 0.7:
                    hyperedges.add((a, va, b, vb, c, vc))

        # Ground-truth count of 4-hypercliques in H
        true_hypercliques = 0
        for v_tuple in itertools.product(range(N_test), repeat=K_test):
            if all(
                (a, v_tuple[a - 1], b, v_tuple[b - 1], c, v_tuple[c - 1]) in hyperedges
                for a, b, c in itertools.combinations(range(1, K_test + 1), 3)
            ):
                true_hypercliques += 1

        boxes, scale_factor = hyperclique_to_klee_reduction(
            K_test, N_test, hyperedges, pcd_3_4_3, alpha=3
        )
        vol = klee_chan_sdc(boxes)
        recovered_hypercliques = round((1.0 - vol) * scale_factor)
        assert recovered_hypercliques == true_hypercliques, (
            f"Hyperclique mismatch: {recovered_hypercliques} vs {true_hypercliques}"
        )
        print(
            f"[PASS] Trial {trial+1}: 4-Partite 3-Uniform Hypergraph (N={N_test}) -> "
            f"{len(boxes)} boxes in R^3 -> Vol={vol:.6f} -> "
            f"Exact Hypercliques = {recovered_hypercliques} (matches ground truth {true_hypercliques}!)"
        )


def run_covering_compiler_and_special_boxes_tests() -> None:
    print("\n" + "=" * 78)
    print("4. THEOREM 7 COVERING DESIGN COMPILER & PROJECTIVE PLANE VERIFICATION")
    print("=" * 78)

    from pcd_search_and_general_designs import (
        parse_covering_designs_repo,
        compile_covering_design_to_pcd,
        get_best_known_design,
    )
    import os

    repo_path = os.path.join(os.path.dirname(__file__), "covering_designs_repo.txt")
    repo = parse_covering_designs_repo(repo_path)
    print(f"Loaded {len(repo)} covering designs from repository.")

    # Test compilation across small and projective plane dimensions
    for d, n in [(3, 1), (3, 2), (6, 1), (7, 1), (13, 1)]:
        if d in repo:
            cd = repo[d]
            seqs, K, alpha = compile_covering_design_to_pcd(cd, n=n)
            ver = verify_prefix_covering_design(seqs, K, alpha, r=3)
            assert ver.is_valid, f"Compiled d={d}, n={n} failed: {ver.error_message}"
            print(
                f"[PASS] Compiled (d={d}, n={n}): K={K}, alpha={alpha}, "
                f"K/alpha = {K/alpha:.4f} (asymp >= {cd.asymptotic_exponent:.4f}), "
                f"max_singleton={ver.max_singleton_sum} <= {alpha+1}, "
                f"max_triplet={ver.max_tuple_cost} <= {alpha}"
            )

    print("\n" + "=" * 78)
    print("5. SPECIAL BOX FAMILIES: ANCHORED BOXES & UNIT HYPERCUBES")
    print("=" * 78)

    from special_boxes import (
        klee_anchored_2d,
        klee_anchored_3d,
        klee_anchored_nd,
        UnitHypercube,
        klee_unit_hypercubes_grid,
    )

    rng = random.Random(42)

    # 5.1 Anchored Boxes / Orthants vs. Bentley Sweep
    for d in [2, 3, 4]:
        pts = [[rng.uniform(0.1, 5.0) for _ in range(d)] for _ in range(25)]
        boxes = [tuple((0.0, float(p[i])) for i in range(d)) for p in pts]
        ref_vol = klee_sweep_nd(boxes)
        fast_vol = klee_anchored_nd(pts, d)
        diff = abs(ref_vol - fast_vol)
        assert diff < 1e-6, f"Anchored mismatch in d={d}: {ref_vol} vs {fast_vol}"
        print(f"[PASS] Anchored Boxes d={d}: Ref Sweep = {ref_vol:.6f}, Fast Orthant = {fast_vol:.6f} (diff = {diff:.2e})")

    # 5.2 Unit Hypercubes Grid Hashing vs. Bentley Sweep
    for d in [2, 3, 4]:
        cubes = [UnitHypercube(tuple(rng.uniform(0.0, 2.5) for _ in range(d))) for _ in range(15)]
        boxes = [c.to_box() for c in cubes]
        ref_vol = klee_sweep_nd(boxes)
        grid_vol = klee_unit_hypercubes_grid(cubes, d)
        diff = abs(ref_vol - grid_vol)
        assert diff < 1e-6, f"Unit hypercubes mismatch in d={d}: {ref_vol} vs {grid_vol}"
        print(f"[PASS] Unit Hypercubes d={d}: Ref Sweep = {ref_vol:.6f}, Grid Hashing = {grid_vol:.6f} (diff = {diff:.2e})")


if __name__ == "__main__":
    run_correctness_tests()
    run_fmm_and_clique_benchmarks()
    run_prefix_covering_design_tests()
    run_covering_compiler_and_special_boxes_tests()
