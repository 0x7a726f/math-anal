"""
Algebraic Fast Matrix Multiplication (FMM) Solver for 2-Slab / Clean Cells
and Hybrid Chan-SDC + FMM Algorithm for Klee's Measure Problem
==========================================================================
Implements:
1. Clean-Cell Factorization Lemma & Vertex-Weighted d-Partite Clique Reduction:
   For any cell C = prod_{i=0}^{d-1} [alpha_i, beta_i] in R^d where every
   intersecting box has k(B, C) <= 2 (only 0-slabs, 1-slabs, and 2-slabs),
   collapses all 1-slabs into 1D interval weights w_{i, r} and reduces the
   remaining 2-slabs to the exact algebraic trace identity:

       Vol(C \\ U B) = Tr(D_1 * A_{12} * D_2 * A_{23} * D_3 * A_{31})

   evaluated via Fast Matrix Multiplication in O(V^{omega * d / 3}) time.

2. klee_hybrid_chan_fmm:
   Hybrid Simplify-Divide-and-Conquer + FMM algorithm that eliminates 1-slabs
   top-down via Chan (2013) and dispatches Clean (2-slab) cells directly to
   the algebraic FMM trace engine.

3. clique_to_klee_reduction:
   Chan's (2010) fine-grained reduction from d-Clique on a V-vertex graph G
   to Klee's Measure Problem in R^d with n = O(V^2) 2-slab boxes, where
       #ordered d-cliques(G) = V^d - Vol(U B_m).
"""

from __future__ import annotations
from typing import List, Tuple, Optional
import math
import itertools
import numpy as np

from klee_algorithms import (
    Box,
    EPS,
    ChanSDCStats,
    validate_boxes,
    klee_1d,
    klee_2d_bentley,
    simplify_cell_1slabs,
    _choose_chan_split_hyperplane,
)


def is_clean_2slab_cell(cell: List[Tuple[float, float]], boxes: List[Box]) -> bool:
    """
    Return True iff every box B in `boxes` has at most 2 active dimensions
    inside `cell` (i.e., k(B, cell) <= 2: no >=3-slabs exist in `cell`).
    """
    d = len(cell)
    for b in boxes:
        active = 0
        for i in range(d):
            if b[i][0] > cell[i][0] + EPS or b[i][1] < cell[i][1] - EPS:
                active += 1
                if active >= 3:
                    return False
    return True


def _partition_dims_three_groups(v_counts: List[int]) -> Tuple[List[int], List[int], List[int]]:
    """
    Partition dimensions {0, ..., d-1} into 3 non-empty groups (G_1, G_2, G_3)
    to minimize max(prod_{i in G_1} v_i, prod_{i in G_2} v_i, prod_{i in G_3} v_i).
    """
    d = len(v_counts)
    if d < 3:
        raise ValueError("FMM 3-group partition requires d >= 3")
    if d == 3:
        return [0], [1], [2]

    # For d <= 10, exact search over 3^d partitions is tiny (<= 59049) and optimal
    if d <= 9:
        best_max = float("inf")
        best_groups = ([0], [1], list(range(2, d)))
        log_v = [math.log(max(1, v)) for v in v_counts]
        for assign in itertools.product((0, 1, 2), repeat=d):
            if 0 not in assign or 1 not in assign or 2 not in assign:
                continue
            # Symmetry breaking: dim 0 in group 0
            if assign[0] != 0:
                continue
            s0 = sum(log_v[i] for i in range(d) if assign[i] == 0)
            s1 = sum(log_v[i] for i in range(d) if assign[i] == 1)
            s2 = sum(log_v[i] for i in range(d) if assign[i] == 2)
            cur_max = max(s0, s1, s2)
            if cur_max < best_max:
                best_max = cur_max
                g0 = [i for i in range(d) if assign[i] == 0]
                g1 = [i for i in range(d) if assign[i] == 1]
                g2 = [i for i in range(d) if assign[i] == 2]
                best_groups = (g0, g1, g2)
        return best_groups

    # Greedy log-load balancing for larger d
    order = sorted(range(d), key=lambda i: v_counts[i], reverse=True)
    groups: List[List[int]] = [[], [], []]
    loads = [0.0, 0.0, 0.0]
    for idx, dim in enumerate(order):
        if idx < 3:
            target = idx
        else:
            target = min(range(3), key=lambda g: loads[g])
        groups[target].append(dim)
        loads[target] += math.log(max(1, v_counts[dim]))
    return groups[0], groups[1], groups[2]


def solve_clean_cell_fmm(
    cell: List[Tuple[float, float]],
    boxes: List[Box],
    stats: Optional[ChanSDCStats] = None,
) -> float:
    """
    Compute the exact volume of the union of `boxes` inside `cell` assuming
    `cell` is a Clean cell (all boxes after 1-slab simplification have k(B, C) <= 2).

    Uses the Clean-Cell Factorization Lemma + Fast Matrix Multiplication:
        Vol(C \\ U B) = Tr(D_1 A_{12} D_2 A_{23} D_3 A_{31})
    """
    d = len(cell)
    if d == 1:
        return klee_1d([b[0] for b in boxes])
    if d == 2:
        return klee_2d_bentley(boxes)

    # First collapse all 0-slabs and 1-slabs via Chan's coordinate compression
    vol_1slabs, comp_cell, two_slabs, fully_covered = simplify_cell_1slabs(
        cell, boxes, stats
    )
    if fully_covered or not two_slabs:
        return vol_1slabs

    # Verify cleanliness (every surviving box must be a 2-slab)
    if not is_clean_2slab_cell(comp_cell, two_slabs):
        raise ValueError("solve_clean_cell_fmm called on a Dirty cell containing a >=3-slab")

    # Build elementary intervals J_{i, 0}, ..., J_{i, v_i - 1} in each dimension i
    coords: List[List[float]] = []
    weights_1d: List[np.ndarray] = []
    coord_to_idx: List[dict] = []
    v_counts: List[int] = []

    for i in range(d):
        ca, cb = comp_cell[i]
        pts = {ca, cb}
        for b in two_slabs:
            if b[i][0] > ca + EPS or b[i][1] < cb - EPS:
                pts.add(max(ca, min(cb, b[i][0])))
                pts.add(max(ca, min(cb, b[i][1])))
        sorted_pts = sorted(pts)
        # Filter near-duplicate coordinates
        dedup = [sorted_pts[0]]
        for p in sorted_pts[1:]:
            if p - dedup[-1] > EPS:
                dedup.append(p)
        coords.append(dedup)
        w = np.array([dedup[k + 1] - dedup[k] for k in range(len(dedup) - 1)], dtype=np.float64)
        weights_1d.append(w)
        coord_to_idx.append({val: idx for idx, val in enumerate(dedup)})
        v_counts.append(len(w))

    if any(v == 0 for v in v_counts):
        return vol_1slabs

    # Build 2D pairwise compatibility matrices M[(i, j)] of shape (v_i, v_j) for 0 <= i < j < d
    # M[(i, j)][r_i, r_j] = True iff NO (i, j)-2-slab covers J_{i, r_i} x J_{j, r_j}
    M = {}
    for i in range(d):
        for j in range(i + 1, d):
            M[(i, j)] = np.ones((v_counts[i], v_counts[j]), dtype=bool)

    def _find_interval_range(dedup: List[float], a: float, b: float) -> Tuple[int, int]:
        # Binary search / scan for indices [l_idx, r_idx) covered by [a, b]
        l_idx = 0
        while l_idx < len(dedup) - 1 and dedup[l_idx + 1] <= a + EPS:
            l_idx += 1
        r_idx = len(dedup) - 1
        while r_idx > l_idx and dedup[r_idx - 1] >= b - EPS:
            r_idx -= 1
        return l_idx, r_idx

    for b in two_slabs:
        active_dims = [
            i
            for i in range(d)
            if b[i][0] > comp_cell[i][0] + EPS or b[i][1] < comp_cell[i][1] - EPS
        ]
        if len(active_dims) == 2:
            i, j = active_dims[0], active_dims[1]
            li, ri = _find_interval_range(coords[i], b[i][0], b[i][1])
            lj, rj = _find_interval_range(coords[j], b[j][0], b[j][1])
            if li < ri and lj < rj:
                M[(i, j)][li:ri, lj:rj] = False

    # Partition dimensions into 3 groups G_1, G_2, G_3
    G1, G2, G3 = _partition_dims_three_groups(v_counts)
    groups = [G1, G2, G3]

    # For each group G_a, enumerate internally valid states (tuples of interval indices)
    # and their product weights W_a
    group_states: List[np.ndarray] = []
    group_weights: List[np.ndarray] = []

    for G in groups:
        grids = [np.arange(v_counts[i], dtype=np.int32) for i in G]
        mesh = np.meshgrid(*grids, indexing="ij")
        # Shape: (N_G, len(G))
        states = np.stack([m.ravel() for m in mesh], axis=-1)
        valid_mask = np.ones(len(states), dtype=bool)
        for idx_u in range(len(G)):
            for idx_v in range(idx_u + 1, len(G)):
                u, v = G[idx_u], G[idx_v]
                if u < v:
                    valid_mask &= M[(u, v)][states[:, idx_u], states[:, idx_v]]
                else:
                    valid_mask &= M[(v, u)][states[:, idx_v], states[:, idx_u]]
        valid_states = states[valid_mask]
        if len(valid_states) == 0:
            # All states in this group are internally blocked -> uncovered volume is 0!
            comp_vol = math.prod(comp_cell[i][1] - comp_cell[i][0] for i in range(d))
            return vol_1slabs + comp_vol

        w_prod = np.ones(len(valid_states), dtype=np.float64)
        for idx_u, u in enumerate(G):
            w_prod *= weights_1d[u][valid_states[:, idx_u]]

        group_states.append(valid_states)
        group_weights.append(w_prod)

    # Build bipartite compatibility matrices A12, A23, A31 between surviving states
    def _build_bipartite_matrix(
        Ga: List[int], states_a: np.ndarray, Gb: List[int], states_b: np.ndarray
    ) -> np.ndarray:
        A = np.ones((len(states_a), len(states_b)), dtype=bool)
        for idx_u, u in enumerate(Ga):
            col_u = states_a[:, idx_u]
            for idx_v, v in enumerate(Gb):
                col_v = states_b[:, idx_v]
                if u < v:
                    A &= M[(u, v)][col_u[:, None], col_v[None, :]]
                else:
                    A &= M[(v, u)].T[col_u[:, None], col_v[None, :]]
        return A.astype(np.float64)

    A12 = _build_bipartite_matrix(G1, group_states[0], G2, group_states[1])
    A23 = _build_bipartite_matrix(G2, group_states[1], G3, group_states[2])
    A31 = _build_bipartite_matrix(G3, group_states[2], G1, group_states[0])

    # Compute Tr(D_1 A_{12} D_2 A_{23} D_3 A_{31}) using fast matrix multiplication:
    # Let B12 = D_1 * A_{12} * D_2, B23 = A_{23} * D_3
    # Then Tr((B12 @ B23) @ A31) = sum_{p, r} (B12 @ B23)_{p, r} * (A31^T)_{p, r}
    W1, W2, W3 = group_weights[0], group_weights[1], group_weights[2]
    B12 = (W1[:, None] * A12) * W2[None, :]
    B23 = A23 * W3[None, :]

    # Fast Matrix Multiplication (GEMM)
    P13 = B12 @ B23
    uncovered_vol = float(np.sum(P13 * A31.T))

    comp_vol = math.prod(comp_cell[i][1] - comp_cell[i][0] for i in range(d))
    covered_in_comp = max(0.0, comp_vol - uncovered_vol)
    return vol_1slabs + covered_in_comp


def klee_hybrid_chan_fmm(
    boxes: List[Box],
    max_fmm_dim: int = 2048,
    stats: Optional[ChanSDCStats] = None,
) -> float:
    """
    Hybrid Chan Simplify-Divide-and-Conquer + Fast Matrix Multiplication solver:
    - Eliminates 0-slabs and 1-slabs at every tree node via Chan's coordinate compression.
    - Whenever a node C is a Clean cell (all remaining boxes are 2-slabs, k(B, C) == 2)
      and the induced FMM matrix dimension max(N_1, N_2, N_3) <= max_fmm_dim,
      evaluates C directly in O(N^omega) via solve_clean_cell_fmm!
    - Otherwise splits along Chan's codimension-2 weighted median hyperplane.
    """
    valid, d = validate_boxes(boxes)
    if not valid:
        return 0.0
    if d == 1:
        return klee_1d([b[0] for b in valid])
    if d == 2:
        return klee_2d_bentley(valid)

    root_cell = [
        (min(b[i][0] for b in valid), max(b[i][1] for b in valid))
        for i in range(d)
    ]

    def recurse(cell: List[Tuple[float, float]], cur_boxes: List[Box], depth: int) -> float:
        if stats is not None:
            stats.recursive_calls += 1
            if depth > stats.max_depth:
                stats.max_depth = depth

        if not cur_boxes:
            if stats is not None:
                stats.leaf_cells += 1
            return 0.0

        vol_1slabs, comp_cell, rem_boxes, fully_covered = simplify_cell_1slabs(
            cell, cur_boxes, stats
        )
        if fully_covered or not rem_boxes:
            if stats is not None:
                stats.leaf_cells += 1
            return vol_1slabs

        # Check if Clean cell (all surviving boxes are 2-slabs)
        if is_clean_2slab_cell(comp_cell, rem_boxes):
            # Estimate effective 1D grid counts v_i
            v_est = []
            for i in range(d):
                ca, cb = comp_cell[i]
                pts = {ca, cb}
                for b in rem_boxes:
                    if b[i][0] > ca + EPS or b[i][1] < cb - EPS:
                        pts.add(round(b[i][0], 9))
                        pts.add(round(b[i][1], 9))
                v_est.append(max(1, len(pts) - 1))
            G1, G2, G3 = _partition_dims_three_groups(v_est)
            n1 = math.prod(v_est[i] for i in G1)
            n2 = math.prod(v_est[i] for i in G2)
            n3 = math.prod(v_est[i] for i in G3)
            if max(n1, n2, n3) <= max_fmm_dim:
                if stats is not None:
                    stats.clean_2slab_cells += 1
                    stats.leaf_cells += 1
                return vol_1slabs + solve_clean_cell_fmm(comp_cell, rem_boxes, stats=None)

        # Otherwise Dirty cell (or huge sparse Clean cell): split via Chan's median rule
        s, h = _choose_chan_split_hyperplane(comp_cell, rem_boxes, depth % d)
        ca, cb = comp_cell[s]
        if h <= ca + EPS or h >= cb - EPS:
            h = 0.5 * (ca + cb)

        left_cell = list(comp_cell)
        left_cell[s] = (ca, h)
        right_cell = list(comp_cell)
        right_cell[s] = (h, cb)

        left_boxes: List[Box] = []
        right_boxes: List[Box] = []
        for b in rem_boxes:
            if b[s][0] < h - EPS:
                bl = list(b)
                bl[s] = (b[s][0], min(h, b[s][1]))
                left_boxes.append(tuple(bl))
            if b[s][1] > h + EPS:
                br = list(b)
                br[s] = (max(h, b[s][0]), b[s][1])
                right_boxes.append(tuple(br))

        return (
            vol_1slabs
            + recurse(left_cell, left_boxes, depth + 1)
            + recurse(right_cell, right_boxes, depth + 1)
        )

    return recurse(root_cell, valid, 0)


# ============================================================================
# Chan's (2010) Reduction: d-Clique -> Klee's Measure Problem in R^d
# ============================================================================

def clique_to_klee_reduction(adj_matrix: np.ndarray, d: int) -> Tuple[List[Box], int]:
    """
    Given an undirected V x V adjacency matrix `adj_matrix` (0-1 with 0 diagonal)
    and target clique size `d`, construct n <= C(d, 2) * V^2 axis-aligned 2-slab
    boxes in [0, V]^d such that:

        (# of ordered d-cliques in G) = V^d - Vol(Union of boxes)
        (# of unordered d-cliques in G) = (V^d - Vol(Union of boxes)) / d!

    Also returns the exact ground-truth ordered d-clique count via brute-force/matrix trace.
    """
    V = adj_matrix.shape[0]
    full_interval = (0.0, float(V))
    boxes: List[Box] = []

    # For every pair of coordinates 0 <= i < j < d and every non-edge (u, v) (including u == v),
    # forbid x_i in [u, u+1] and x_j in [v, v+1] via a 2-slab box.
    for i in range(d):
        for j in range(i + 1, d):
            for u in range(V):
                for v in range(V):
                    if u == v or adj_matrix[u, v] == 0:
                        b = [full_interval] * d
                        b[i] = (float(u), float(u + 1))
                        b[j] = (float(v), float(v + 1))
                        boxes.append(tuple(b))

    # Ground-truth count of ordered d-cliques
    ordered_cliques = 0
    for tup in itertools.product(range(V), repeat=d):
        if all(adj_matrix[tup[i], tup[j]] == 1 for i in range(d) for j in range(i + 1, d)):
            ordered_cliques += 1

    return boxes, ordered_cliques
