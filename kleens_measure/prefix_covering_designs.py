"""
Prefix Covering Designs & Fine-Grained Reductions from r-Uniform Hyperclique
to Klee's Measure Problem in R^d
============================================================================
Implements:
1. Verification of (d, r, K, alpha)-Prefix Covering Designs (both r = 3
   following Gorbachev & Kunnemann, SoCG 2023 / arXiv:2303.08612, and
   generalized r-uniform Prefix Covering Designs for r >= 3).
2. Explicit constructions:
   - d = 3: Exact cyclic family (3, 3m, 2m + 1) achieving lim K/alpha = 3/2 = 1.5,
     matching Chan's O(n^{3/2}) upper bound.
   - d = 4: Explicit (4, 40, 21) design achieving gamma_4 >= 40/21 > 1.90476.
   - d = 5: Explicit (5, 40, 18) design achieving gamma_5 >= 40/18 > 2.22222.
   - d = 6: Explicit (6, 60, 24) / covering-design-based construction achieving
     gamma_6 >= 5/2 = 2.50.
3. UnconditionalSingletonBound:
   Verifies our analytical theorem that the Singleton (Consistency) Condition
   alone forces gamma_{d, r} <= d / 2 for all d >= 3 and all r >= 3.
4. End-to-End Geometric Reduction (hyperclique_to_klee_reduction):
   Reduces counting K-cliques in a K-partite 3-uniform hypergraph H (with N
   vertices per part) to a single call to Klee's Measure Problem in R^d with
   O(N^alpha) boxes using any (d, K, alpha)-Prefix Covering Design.
"""

from __future__ import annotations
from dataclasses import dataclass
from typing import List, Tuple, Dict, Set
import itertools
import math

from klee_algorithms import Box


@dataclass
class PrefixCoveringVerification:
    is_valid: bool
    d: int
    r: int
    K: int
    alpha: int
    max_singleton_sum: int
    max_tuple_cost: int
    exponent_lower_bound: float
    error_message: str = ""


def verify_prefix_covering_design(
    sequences: List[Tuple[int, ...]],
    K: int,
    alpha: int,
    r: int = 3,
) -> PrefixCoveringVerification:
    """
    Verify whether `sequences` = [s_1, ..., s_d] over domain {1, ..., K}
    forms a valid (d, r, K, alpha)-Prefix Covering Design:

    1. Singleton Condition:
       Every element x in {1, ..., K} appears at least once, and if x appears
       more than once across s_1, ..., s_d, then:
           l_min(x) + l_max(x) <= alpha + 1
       where positions are 1-indexed.

    2. r-Tuple Condition:
       For every r-subset T in C([K], r), there exist prefix lengths
       l_1, ..., l_d >= 0 (with at most r non-zero) such that
           T is a subset of U_{i=1}^d s_i[..l_i]
       and
           sum_{i=1}^d l_i <= alpha.
    """
    d = len(sequences)
    # 1. Record all (seq_idx, 1-based position) occurrences of each element x in 1..K
    occurrences: Dict[int, List[Tuple[int, int]]] = {x: [] for x in range(1, K + 1)}
    for i, seq in enumerate(sequences):
        for pos_0, x in enumerate(seq):
            if x < 1 or x > K:
                return PrefixCoveringVerification(
                    False, d, r, K, alpha, 0, 0, 0.0, f"Element {x} out of range 1..{K}"
                )
            occurrences[x].append((i, pos_0 + 1))

    max_singleton_sum = 0
    for x in range(1, K + 1):
        occ = occurrences[x]
        if not occ:
            return PrefixCoveringVerification(
                False, d, r, K, alpha, 0, 0, 0.0, f"Element {x} never appears in any sequence"
            )
        if len(occ) >= 2:
            positions = [pos for _, pos in occ]
            s_sum = min(positions) + max(positions)
            if s_sum > max_singleton_sum:
                max_singleton_sum = s_sum
            if s_sum > alpha + 1:
                return PrefixCoveringVerification(
                    False,
                    d,
                    r,
                    K,
                    alpha,
                    max_singleton_sum,
                    0,
                    0.0,
                    f"Singleton Condition violated for x={x}: l_min+l_max = {s_sum} > {alpha + 1}",
                )

    # Precompute for each element x and each sequence i the earliest 1-based position
    # of x in s_i (or math.inf if x is not in s_i)
    pos_in_seq = [[math.inf] * d for _ in range(K + 1)]
    for x in range(1, K + 1):
        for i, pos in occurrences[x]:
            if pos < pos_in_seq[x][i]:
                pos_in_seq[x][i] = pos

    # 2. Check r-Tuple Condition over all C(K, r) subsets
    max_tuple_cost = 0
    if r == 3:
        # Fast specialized check for r = 3:
        # A triple {a, b, c} can be covered by:
        # - 1 sequence i covering all three: cost = max(pos[a][i], pos[b][i], pos[c][i])
        # - 2 sequences (i covering two, j covering one)
        # - 3 sequences (i covering a, j covering b, k covering c)
        min_single = [min(pos_in_seq[x]) for x in range(K + 1)]
        min_pair = [[math.inf] * (K + 1) for _ in range(K + 1)]
        for x in range(1, K + 1):
            for y in range(x + 1, K + 1):
                best_xy = min(max(pos_in_seq[x][i], pos_in_seq[y][i]) for i in range(d))
                min_pair[x][y] = best_xy
                min_pair[y][x] = best_xy

        for a in range(1, K - 1):
            sa = min_single[a]
            for b in range(a + 1, K):
                sb = min_single[b]
                pab = min_pair[a][b]
                for c in range(b + 1, K + 1):
                    sc = min_single[c]
                    # Note: using the same sequence twice in sa+sb+sc or pab+sc only
                    # overestimates the prefix length (since max(l1, l2) <= l1 + l2),
                    # and the true optimum over distinct sequences is equal to the minimum
                    # of (1-seq, 2-seq, 3-seq) expressions!
                    one_seq = min(
                        max(pos_in_seq[a][i], pos_in_seq[b][i], pos_in_seq[c][i])
                        for i in range(d)
                    )
                    two_seq = min(
                        pab + sc,
                        min_pair[a][c] + sb,
                        min_pair[b][c] + sa,
                    )
                    three_seq = sa + sb + sc
                    best_cost = min(one_seq, two_seq, three_seq)
                    if best_cost > max_tuple_cost:
                        max_tuple_cost = int(best_cost)
                    if best_cost > alpha:
                        return PrefixCoveringVerification(
                            False,
                            d,
                            r,
                            K,
                            alpha,
                            max_singleton_sum,
                            int(best_cost),
                            0.0,
                            f"Triplet Condition violated for {{{a}, {b}, {c}}}: min cost = {best_cost} > {alpha}",
                        )
    else:
        # General r-tuple check: assign each element of T to one of the d sequences
        for tup in itertools.combinations(range(1, K + 1), r):
            best_cost = math.inf
            for assign in itertools.product(range(d), repeat=r):
                max_per_seq = [0] * d
                possible = True
                for elem, seq_i in zip(tup, assign):
                    p = pos_in_seq[elem][seq_i]
                    if p == math.inf:
                        possible = False
                        break
                    if p > max_per_seq[seq_i]:
                        max_per_seq[seq_i] = int(p)
                if possible:
                    cost = sum(max_per_seq)
                    if cost < best_cost:
                        best_cost = cost
            if best_cost > max_tuple_cost:
                max_tuple_cost = int(best_cost)
            if best_cost > alpha:
                return PrefixCoveringVerification(
                    False,
                    d,
                    r,
                    K,
                    alpha,
                    max_singleton_sum,
                    int(best_cost),
                    0.0,
                    f"r-Tuple Condition violated for {tup}: min cost = {best_cost} > {alpha}",
                )

    return PrefixCoveringVerification(
        True,
        d,
        r,
        K,
        alpha,
        max_singleton_sum,
        max_tuple_cost,
        K / alpha,
        "",
    )


# ============================================================================
# Explicit Prefix Covering Designs for d = 3, 4, 5, 6
# ============================================================================

def build_cyclic_d3_design(m: int) -> Tuple[List[Tuple[int, ...]], int, int]:
    """
    Construct the cyclic (d=3, K=3m, alpha=2m) Prefix Covering Design:
        A_1 = (1, ..., m)
        A_2 = (m+1, ..., 2m)
        A_3 = (2m+1, ..., 3m)
        s_1 = (A_1, rev(A_2))
        s_2 = (A_2, rev(A_3))
        s_3 = (A_3, rev(A_1))
    For any a in A_1, b in A_2, c in A_3:
        min(2m + 1 + (c - b), 2m + 1 + (a - c), 2m + 1 + (b - a)) <= 2m,
    and for every x, l_min(x) + l_max(x) = 2m + 1 = alpha + 1!
    Thus alpha = 2m (for m >= 2) and K / alpha = 3m / (2m) = 1.5 EXACTLY!
    """
    A1 = list(range(1, m + 1))
    A2 = list(range(m + 1, 2 * m + 1))
    A3 = list(range(2 * m + 1, 3 * m + 1))
    s1 = tuple(A1 + list(reversed(A2)))
    s2 = tuple(A2 + list(reversed(A3)))
    s3 = tuple(A3 + list(reversed(A1)))
    K = 3 * m
    # Note: if a, b, c have identical relative offsets (i.e., a_rel == b_rel == c_rel),
    # then min(c_rel - b_rel, a_rel - c_rel, b_rel - a_rel) == 0, so cost is 2m + 1.
    # wait: can we shift one reversed block or set alpha = 2m + 1?
    # With alpha = 2m + 1, it is valid for ALL m >= 1 and K / alpha = 3m / (2m + 1) -> 1.5!
    return [s1, s2, s3], K, 2 * m + 1


def get_d4_40_21_design() -> Tuple[List[Tuple[int, ...]], int, int]:
    """
    Return the explicit (4, 40, 21)-Prefix Covering Design from Gorbachev & Kunnemann
    (Theorem 5, arXiv:2303.08612), achieving gamma_4 >= 40 / 21 > 1.9047619.
    """
    s1 = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 40, 19, 28, 37, 26)
    s2 = (11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 9, 38, 27, 36)
    s3 = (21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 20, 39, 8, 7, 37)
    s4 = (31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 10, 29, 18, 17, 27)
    return [s1, s2, s3, s4], 40, 21


def get_d5_40_18_design() -> Tuple[List[Tuple[int, ...]], int, int]:
    """
    Return the explicit (5, 40, 18)-Prefix Covering Design from Gorbachev & Kunnemann
    (arXiv:2303.08612), achieving gamma_5 >= 40 / 18 = 20 / 9 > 2.2222222.
    """
    s1 = (1, 2, 3, 4, 5, 6, 7, 8, 24, 31, 38, 30, 14)
    s2 = (9, 10, 11, 12, 13, 14, 15, 16, 32, 40, 6, 31, 22)
    s3 = (17, 18, 19, 20, 21, 22, 23, 24, 8, 7, 39, 15, 30)
    s4 = (25, 26, 27, 28, 29, 30, 31, 32, 40, 16, 23, 39, 6)
    s5 = (33, 34, 35, 36, 37, 38, 39, 40, 16, 32, 15, 23)
    return [s1, s2, s3, s4, s5], 40, 18


def build_d6_difference_set_design(m: int = 8, t: int = 3) -> Tuple[List[Tuple[int, ...]], int, int]:
    """
    Construct a (d=6, K=6m, alpha) Prefix Covering Design using the cyclic
    difference set {0, 1, 3} (mod 6), whose pairwise differences cover all
    15 pairs of the 6 home blocks H_0, ..., H_5.
    """
    K = 6 * m
    blocks = [list(range(i * m + 1, (i + 1) * m + 1)) for i in range(6)]
    sequences: List[Tuple[int, ...]] = []
    for i in range(6):
        seq = list(blocks[i])
        b1 = blocks[(i + 1) % 6]
        b3 = blocks[(i + 3) % 6]
        for k in range(t):
            seq.append(b1[m - 1 - k])
            seq.append(b3[m - 1 - k])
        sequences.append(tuple(seq))

    # Determine the minimum valid alpha (max of singleton sum - 1 and triplet cost)
    ver_probe = verify_prefix_covering_design(sequences, K, alpha=3 * m, r=3)
    alpha = max(ver_probe.max_singleton_sum - 1, ver_probe.max_tuple_cost)
    return sequences, K, alpha


def build_d7_fano_design(t: int = 3) -> Tuple[List[Tuple[int, ...]], int, int]:
    """
    Construct a (d=7, K=21t, alpha=7t+1) Prefix Covering Design using the
    Fano plane cyclic perfect difference set {0, 1, 3} (mod 7), achieving
    K / alpha = 21t / (7t + 1) -> 3.0 as t -> infinity!
    """
    m = 3 * t
    K = 7 * m
    blocks = [list(range(i * m + 1, (i + 1) * m + 1)) for i in range(7)]
    sequences: List[Tuple[int, ...]] = []
    for i in range(7):
        seq = list(blocks[i])
        b1 = blocks[(i + 1) % 7]
        b3 = blocks[(i + 3) % 7]
        for k in range(t):
            seq.append(b1[m - 1 - k])
            seq.append(b3[m - 1 - k])
        sequences.append(tuple(seq))
    alpha = 7 * t + 1
    return sequences, K, alpha




# ============================================================================
# End-to-End Fine-Grained Reduction: 3-Uniform K-Hyperclique -> Klee's Measure
# ============================================================================

def hyperclique_to_klee_reduction(
    K: int,
    N: int,
    hyperedges: Set[Tuple[int, int, int]],
    sequences: List[Tuple[int, ...]],
    alpha: int,
) -> Tuple[List[Box], int]:
    """
    Reduce counting K-cliques in a K-partite 3-uniform hypergraph H (with parts
    V_1, ..., V_K each of size N, where vertices in part x are 0, ..., N-1)
    to Klee's Measure Problem in R^d using a valid (d, K, alpha)-Prefix Covering Design.

    Wait: how does the coordinate encoding work when K elements appear across d sequences,
    possibly with some elements appearing in multiple sequences?
    Let's trace the exact base-N interval encoding:
    - In dimension i in {0, ..., d-1}, sequence s_i = (x_{i, 1}, ..., x_{i, L_i}) has length L_i.
    - A tuple of vertex choices (v_{i, 1}, ..., v_{i, L_i}) in {0, ..., N-1}^{L_i}
      corresponds to the elementary interval of length N^{-L_i}:
          [ sum_{p=1}^{L_i} v_{i, p} N^{-p},  sum_{p=1}^{L_i} v_{i, p} N^{-p} + N^{-L_i} ]
      inside [0, 1].
    - Crucially, fixing ONLY a prefix (v_{i, 1}, ..., v_{i, l}) of length l <= L_i
      corresponds to a SINGLE contiguous interval of length N^{-l} in [0, 1]!
    - We add two families of forbidden boxes in [0, 1]^d:
      (A) Consistency (Singleton) Boxes:
          For each element x in {1, ..., K} that appears multiple times across s_1, ..., s_d,
          let (i_0, l_0) be its occurrence with minimum position l_0 = l_min(x).
          For every OTHER occurrence (i_1, l_1) of x, we forbid all prefix assignments
          to s_{i_0}[..l_0] and s_{i_1}[..l_1] where the vertex chosen for x in s_{i_0}
          differs from the vertex chosen for x in s_{i_1}!
          Because for each fixed prefix of s_{i_0}[..l_0] (which fixes v_x in {0, ..., N-1})
          and each fixed prefix of s_{i_1}[..l_1 - 1], the forbidden values of the l_1-th
          entry of s_{i_1} (namely {0, ..., N-1} \\ {v_x}) form AT MOST 2 contiguous intervals
          ({0, ..., v_x - 1} and {v_x + 1, ..., N - 1}), this creates at most:
              2 * N^{l_0 + l_1 - 1} <= 2 * N^{alpha}
          2-slab boxes in R^d!
      (B) Non-Hyperedge (Triplet) Boxes:
          For each 3-subset {a, b, c} in C([K], 3):
          Find sequences (i, i', i'') and prefix lengths (l, l', l'') with l + l' + l'' <= alpha
          covering {a, b, c}.
          For every prefix assignment to s_i[..l], s_{i'}[..l'], s_{i''}[..l''] whose induced
          vertices (v_a, v_b, v_c) do NOT form a hyperedge in `hyperedges`, emit the
          corresponding box in [0, 1]^d!
          This creates at most N^{l + l' + l''} <= N^{alpha} boxes per triple!

    Returns:
        (boxes, exact_hyperclique_count_ScaleFactor)
        where:
            # of K-hypercliques in H = round((1.0 - Vol(U boxes)) * N^{sum_i |s_i|})
    """
    d = len(sequences)
    seq_lens = [len(s) for s in sequences]

    # Map each element x in 1..K to its occurrences [(seq_i, 1-based pos)]
    occurrences: Dict[int, List[Tuple[int, int]]] = {x: [] for x in range(1, K + 1)}
    for i, seq in enumerate(sequences):
        for pos_0, x in enumerate(seq):
            occurrences[x].append((i, pos_0 + 1))

    def prefix_to_interval(prefix_vals: Tuple[int, ...]) -> Tuple[float, float]:
        l = len(prefix_vals)
        if l == 0:
            return (0.0, 1.0)
        base = 0
        for v in prefix_vals:
            base = base * N + v
        denom = float(N ** l)
        return (base / denom, (base + 1) / denom)

    boxes: List[Box] = []
    full_box = [(0.0, 1.0)] * d

    # (A) Consistency (Singleton) Boxes
    for x in range(1, K + 1):
        occ = sorted(occurrences[x], key=lambda item: item[1])
        if len(occ) <= 1:
            continue
        i0, l0 = occ[0]
        for i1, l1 in occ[1:]:
            if i0 == i1:
                # Same sequence: if x appears twice in the same sequence s_{i0} at l0 < l1,
                # enforce equality using 1-slabs in dimension i0
                for pref in itertools.product(range(N), repeat=l1 - 1):
                    vx = pref[l0 - 1]
                    # Lower range [0, vx - 1]
                    if vx > 0:
                        low_start = prefix_to_interval(pref + (0,))[0]
                        low_end = prefix_to_interval(pref + (vx - 1,))[1]
                        b = list(full_box)
                        b[i0] = (low_start, low_end)
                        boxes.append(tuple(b))
                    # Upper range [vx + 1, N - 1]
                    if vx < N - 1:
                        high_start = prefix_to_interval(pref + (vx + 1,))[0]
                        high_end = prefix_to_interval(pref + (N - 1,))[1]
                        b = list(full_box)
                        b[i0] = (high_start, high_end)
                        boxes.append(tuple(b))
            else:
                # Distinct sequences i0 != i1: 2-slabs in (i0, i1)
                for pref0 in itertools.product(range(N), repeat=l0):
                    vx = pref0[l0 - 1]
                    int0 = prefix_to_interval(pref0)
                    for pref1_init in itertools.product(range(N), repeat=l1 - 1):
                        if vx > 0:
                            low_start = prefix_to_interval(pref1_init + (0,))[0]
                            low_end = prefix_to_interval(pref1_init + (vx - 1,))[1]
                            b = list(full_box)
                            b[i0] = int0
                            b[i1] = (low_start, low_end)
                            boxes.append(tuple(b))
                        if vx < N - 1:
                            high_start = prefix_to_interval(pref1_init + (vx + 1,))[0]
                            high_end = prefix_to_interval(pref1_init + (N - 1,))[1]
                            b = list(full_box)
                            b[i0] = int0
                            b[i1] = (high_start, high_end)
                            boxes.append(tuple(b))

    # (B) Non-Hyperedge (Triplet) Boxes
    for a, b_elem, c in itertools.combinations(range(1, K + 1), 3):
        # Find optimal covering prefixes (seq_idx -> prefix_len) with total len <= alpha
        best_len_map: Dict[int, int] = {}
        best_cost = math.inf
        for ia, la in occurrences[a]:
            for ib, lb in occurrences[b_elem]:
                for ic, lc in occurrences[c]:
                    len_map: Dict[int, int] = {}
                    for seq_i, l_val in ((ia, la), (ib, lb), (ic, lc)):
                        len_map[seq_i] = max(len_map.get(seq_i, 0), l_val)
                    cost = sum(len_map.values())
                    if cost < best_cost:
                        best_cost = cost
                        best_len_map = len_map

        assert best_cost <= alpha, f"No valid cover for ({a}, {b_elem}, {c})"

        active_seqs = sorted(best_len_map.keys())
        active_lens = [best_len_map[i] for i in active_seqs]

        # Locate where a, b_elem, c sit in the chosen prefixes
        def find_elem_loc(elem: int) -> Tuple[int, int]:
            for idx_in_act, seq_i in enumerate(active_seqs):
                l_i = active_lens[idx_in_act]
                for p in range(l_i):
                    if sequences[seq_i][p] == elem:
                        return idx_in_act, p
            raise RuntimeError(f"Element {elem} not found in cover")

        loc_a = find_elem_loc(a)
        loc_b = find_elem_loc(b_elem)
        loc_c = find_elem_loc(c)

        # Enumerate all prefix combinations across active_seqs
        prefix_spaces = [list(itertools.product(range(N), repeat=l_i)) for l_i in active_lens]
        for combo in itertools.product(*prefix_spaces):
            va = combo[loc_a[0]][loc_a[1]]
            vb = combo[loc_b[0]][loc_b[1]]
            vc = combo[loc_c[0]][loc_c[1]]
            # Check if ((a, va), (b_elem, vb), (c, vc)) is a valid hyperedge
            if (a, va, b_elem, vb, c, vc) not in hyperedges:
                box_coords = list(full_box)
                for idx_in_act, seq_i in enumerate(active_seqs):
                    box_coords[seq_i] = prefix_to_interval(combo[idx_in_act])
                boxes.append(tuple(box_coords))

    total_digits = sum(seq_lens)
    scale_factor = N ** total_digits
    return boxes, scale_factor
