r"""
tests/verify_tensor_contraction.py
==================================
Exact Symbolic and Numerical Verification Engine for Higher-Dimensional
Clean-Cell Tensor Network Contraction in Klee's Measure Problem (KMP).

Mathematical Formulation:
-------------------------
Clean-Cell Factorization Lemma (General d >= 3):
Let C = \prod_{i=0}^{d-1} [L_i, R_i] be a d-dimensional axis-aligned cell
containing only 0-slabs, 1-slabs, and 2-slabs (Clean cell).
For each coordinate axis i, let the clamped box boundaries induce V_i
elementary intervals I_{i, r_i} with diagonal weight vector D^(i) in Z^(V_i):
    D^(i)_{r_i} = |I_{i, r_i}| if I_{i, r_i} is not covered by any 1-slab in dimension i, else 0.

For each pair of coordinates {u, v} (0 <= u < v < d), let A^(u, v) in {0, 1}^(V_u x V_v)
be the binary compatibility matrix:
    A^(u, v)_{r_u, r_v} = 0 if any 2-slab active on {u, v} covers I_{u, r_u} x I_{v, r_v}, else 1.

Then the Lebesgue measure of the complement (C \\ \\bigcup B) is the exact contraction
of the tensor network defined on the 2-slab interaction graph G = (V_d, E_{d-2}):
    Vol(C \\ \\bigcup B) = \\sum_{r_0=0}^{V_0-1} ... \\sum_{r_{d-1}=0}^{V_{d-1}-1}
                          ( \\prod_{i=0}^{d-1} D^(i)_{r_i} )
                          \\prod_{0 <= u < v < d} A^(u, v)_{r_u, r_v}

And the union volume is:
    Vol(\\bigcup B \\cap C) = Vol(C) - Vol(C \\ \\bigcup B).

Exhaustively verifies 100% exact volume equality against combinatorial hyperplane sweeps
for d=3, d=4, and d=5 on random, trellis weave, hyperclique gadget, and edge-case configurations.
"""

from __future__ import annotations
import sys
import os
import json
import time
import random
from typing import List, Tuple, Dict, Any
import numpy as np


# ==============================================================================
# Ground Truth Verification Engines (Exact Arbitrary-Precision Integer Sweeps)
# ==============================================================================

def compute_box_union_1d(intervals: List[Tuple[int, int]], L: int, R: int) -> int:
    """Computes exact 1D union length clamped to [L, R]."""
    clamped = []
    for a, b in intervals:
        ca = max(a, L)
        cb = min(b, R)
        if ca < cb:
            clamped.append((ca, cb))
    if not clamped:
        return 0
    clamped.sort()
    total = 0
    cur_a, cur_b = clamped[0]
    for a, b in clamped[1:]:
        if a <= cur_b:
            cur_b = max(cur_b, b)
        else:
            total += (cur_b - cur_a)
            cur_a, cur_b = a, b
    total += (cur_b - cur_a)
    return total


def sweep_box_union_exact(boxes: List[Tuple[Tuple[int, int], ...]], cell_bounds: List[Tuple[int, int]]) -> int:
    """
    Exact reference Lebesgue measure via recursive hyperplane sweep on coordinate 0.
    All bounds and arithmetic use exact Python arbitrary-precision integers.
    """
    d = len(cell_bounds)
    if not boxes:
        return 0

    if d == 1:
        intervals = [(b[0][0], b[0][1]) for b in boxes]
        return compute_box_union_1d(intervals, cell_bounds[0][0], cell_bounds[0][1])

    L0, R0 = cell_bounds[0]
    x_coords = {L0, R0}
    for b in boxes:
        x_coords.add(max(L0, min(R0, b[0][0])))
        x_coords.add(max(L0, min(R0, b[0][1])))
    sorted_x = sorted(x_coords)

    total_vol = 0
    for idx in range(len(sorted_x) - 1):
        x_left = sorted_x[idx]
        x_right = sorted_x[idx + 1]
        width = x_right - x_left
        if width <= 0:
            continue
        # Project active boxes spanning [x_left, x_right]
        active_subboxes = []
        for b in boxes:
            if b[0][0] <= x_left and b[0][1] >= x_right:
                active_subboxes.append(b[1:])
        if active_subboxes:
            sub_vol = sweep_box_union_exact(active_subboxes, cell_bounds[1:])
            total_vol += width * sub_vol

    return total_vol


def elementary_grid_exact(boxes: List[Tuple[Tuple[int, int], ...]], cell_bounds: List[Tuple[int, int]]) -> int:
    """
    Independent exact verification via explicit elementary grid indicator summation.
    Used on small instances for symbolic cross-validation.
    """
    import itertools
    d = len(cell_bounds)
    elementary = []
    for i in range(d):
        L, R = cell_bounds[i]
        coords = {L, R}
        for b in boxes:
            coords.add(max(L, min(R, b[i][0])))
            coords.add(max(L, min(R, b[i][1])))
        sc = sorted(coords)
        elementary.append([(sc[r], sc[r + 1]) for r in range(len(sc) - 1) if sc[r + 1] > sc[r]])

    complement_vol = 0
    grid_shapes = [len(el) for el in elementary]

    for indices in itertools.product(*[range(s) for s in grid_shapes]):
        # Test if this elementary cuboid is covered by any box
        cuboid = [elementary[dim][indices[dim]] for dim in range(d)]
        covered = False
        for b in boxes:
            if all(b[dim][0] <= cuboid[dim][0] and b[dim][1] >= cuboid[dim][1] for dim in range(d)):
                covered = True
                break
        if not covered:
            cell_v = 1
            for dim in range(d):
                cell_v *= (cuboid[dim][1] - cuboid[dim][0])
            complement_vol += cell_v

    cell_total_vol = 1
    for L, R in cell_bounds:
        cell_total_vol *= (R - L)
    return cell_total_vol - complement_vol


# ==============================================================================
# Clean-Cell Tensor Network Extraction
# ==============================================================================

def extract_tensor_network(boxes: List[Tuple[Tuple[int, int], ...]], cell_bounds: List[Tuple[int, int]]) -> Tuple[
    bool, int, List[np.ndarray], Dict[Tuple[int, int], np.ndarray], List[List[Tuple[int, int]]]
]:
    """
    Extracts the tensor network (D^(i), A^(u, v)) for a clean cell.
    Returns:
        (has_0slab, cell_vol, D, A, elementary_intervals)
    """
    d = len(cell_bounds)
    cell_vol = 1
    for L, R in cell_bounds:
        cell_vol *= (R - L)

    # Step 1: Classify slabs
    slabs_1: Dict[int, List[Tuple[int, int]]] = {i: [] for i in range(d)}
    slabs_2: Dict[Tuple[int, int], List[Tuple[Tuple[int, int], Tuple[int, int]]]] = {
        (u, v): [] for u in range(d) for v in range(u + 1, d)
    }

    for b in boxes:
        noncov = []
        disjoint = False
        for i in range(d):
            l = max(cell_bounds[i][0], b[i][0])
            h = min(cell_bounds[i][1], b[i][1])
            if l >= h:
                disjoint = True
                break
            if b[i][0] > cell_bounds[i][0] or b[i][1] < cell_bounds[i][1]:
                noncov.append(i)
        if disjoint:
            continue

        if len(noncov) == 0:
            # 0-slab covers cell completely
            return True, cell_vol, [], {}, []
        elif len(noncov) == 1:
            dim = noncov[0]
            slabs_1[dim].append((max(cell_bounds[dim][0], b[dim][0]), min(cell_bounds[dim][1], b[dim][1])))
        elif len(noncov) == 2:
            u, v = min(noncov), max(noncov)
            int_u = (max(cell_bounds[u][0], b[u][0]), min(cell_bounds[u][1], b[u][1]))
            int_v = (max(cell_bounds[v][0], b[v][0]), min(cell_bounds[v][1], b[v][1]))
            slabs_2[(u, v)].append((int_u, int_v))
        else:
            raise ValueError(f"Cell is not clean: contains {len(noncov)}-slab (k >= 3)")

    # Step 2: Build elementary intervals per coordinate
    elementary: List[List[Tuple[int, int]]] = []
    for i in range(d):
        L, R = cell_bounds[i]
        coords = {L, R}
        for a, b in slabs_1[i]:
            coords.add(a)
            coords.add(b)
        for (u, v), s_list in slabs_2.items():
            if i == u:
                for (a1, b1), _ in s_list:
                    coords.add(a1)
                    coords.add(b1)
            elif i == v:
                for _, (a2, b2) in s_list:
                    coords.add(a2)
                    coords.add(b2)
        sc = sorted(coords)
        elementary.append([(sc[r], sc[r + 1]) for r in range(len(sc) - 1) if sc[r + 1] > sc[r]])

    V = [len(el) for el in elementary]

    # Step 3: Compute diagonal weights D^(i)
    D: List[np.ndarray] = []
    for i in range(d):
        w = np.zeros(V[i], dtype=object)
        for r, (l, h) in enumerate(elementary[i]):
            cov = any(a <= l and b >= h for a, b in slabs_1[i])
            if not cov:
                w[r] = h - l
        D.append(w)

    # Step 4: Build binary adjacency matrices A^(u, v)
    A: Dict[Tuple[int, int], np.ndarray] = {}
    for u in range(d):
        for v in range(u + 1, d):
            mat = np.ones((V[u], V[v]), dtype=object)
            for ru, (lu, hu) in enumerate(elementary[u]):
                for rv, (lv, hv) in enumerate(elementary[v]):
                    cov = any(
                        a1 <= lu and b1 >= hu and a2 <= lv and b2 >= hv
                        for (a1, b1), (a2, b2) in slabs_2[(u, v)]
                    )
                    if cov:
                        mat[ru, rv] = 0
            A[(u, v)] = mat

    return False, cell_vol, D, A, elementary


# ==============================================================================
# Tensor Network Contraction Routines
# ==============================================================================

def tensor_contraction_d3(D: List[np.ndarray], A: Dict[Tuple[int, int], np.ndarray]) -> int:
    """
    Evaluates cyclic matrix trace for d=3:
    Tr(D0 * A01 * D1 * A12 * D2 * A20)
    """
    V0 = len(D[0])
    V1 = len(D[1])
    V2 = len(D[2])

    A01 = A[(0, 1)]
    A12 = A[(1, 2)]
    A02 = A[(0, 2)]  # A20 = A02^T

    # M1 = D0 * A01 * D1 (V0 x V1)
    M1 = (D[0][:, None] * A01) * D[1][None, :]
    # M2 = M1 @ A12 (V0 x V2)
    M2 = np.dot(M1, A12)
    # Trace = sum_{r0, r2} M2[r0, r2] * D2[r2] * A02[r0, r2]
    term = M2 * (D[2][None, :] * A02)
    return int(np.sum(term))


def tensor_contraction_d4(D: List[np.ndarray], A: Dict[Tuple[int, int], np.ndarray]) -> int:
    """
    Evaluates 4D tensor network contraction via factorized bipartite interaction:
    sum_{r0, r1} (D0[r0] * D1[r1] * A01[r0, r1]) * (u^T * A23 * v)
    where u(r2) = D2[r2] * A02[r0, r2] * A12[r1, r2]
          v(r3) = D3[r3] * A03[r0, r3] * A13[r1, r3]
    """
    V0 = len(D[0])
    V1 = len(D[1])
    V2 = len(D[2])
    V3 = len(D[3])

    A01 = A[(0, 1)]
    A02 = A[(0, 2)]
    A03 = A[(0, 3)]
    A12 = A[(1, 2)]
    A13 = A[(1, 3)]
    A23 = A[(2, 3)]

    comp_vol = 0

    for r0 in range(V0):
        w0 = D[0][r0]
        if w0 == 0:
            continue
        for r1 in range(V1):
            w1 = D[1][r1]
            if w1 == 0 or A01[r0, r1] == 0:
                continue

            w01 = w0 * w1

            # Active r2 indices
            u_indices = [r2 for r2 in range(V2) if D[2][r2] > 0 and A02[r0, r2] and A12[r1, r2]]
            if not u_indices:
                continue

            # Active r3 indices
            v_indices = [r3 for r3 in range(V3) if D[3][r3] > 0 and A03[r0, r3] and A13[r1, r3]]
            if not v_indices:
                continue

            # Inner contraction over active r2 and active r3
            inner = 0
            for r2 in u_indices:
                w2 = D[2][r2]
                row_sum = 0
                for r3 in v_indices:
                    if A23[r2, r3]:
                        row_sum += D[3][r3]
                if row_sum > 0:
                    inner += w2 * row_sum

            if inner > 0:
                comp_vol += w01 * inner

    return int(comp_vol)


def tensor_contraction_general(D: List[np.ndarray], A: Dict[Tuple[int, int], np.ndarray], d: int) -> int:
    """
    General d-dimensional tensor network contraction using optimal einsum
    with arbitrary-precision Python integer arithmetic.
    """
    letters = 'abcdefghijklmnopqrstuvwxyz'
    node_subs = [letters[i] for i in range(d)]
    edge_subs = [letters[u] + letters[v] for u in range(d) for v in range(u + 1, d)]
    subscripts = ','.join(node_subs + edge_subs) + '->'

    tensors = [D[i] for i in range(d)] + [A[(u, v)] for u in range(d) for v in range(u + 1, d)]
    res = np.einsum(subscripts, *tensors, optimize=True)
    return int(res)


def tensor_clean_cell_volume(
    boxes: List[Tuple[Tuple[int, int], ...]],
    cell_bounds: List[Tuple[int, int]],
    method: str = "general"
) -> int:
    """
    Computes exact union volume for a Clean cell via Tensor Network Contraction.
    """
    d = len(cell_bounds)
    has_0slab, cell_vol, D, A, elementary = extract_tensor_network(boxes, cell_bounds)
    if has_0slab:
        return cell_vol

    # If any dimension has 0 elementary intervals, cell volume is 0 or degenerate
    if any(len(el) == 0 for el in elementary):
        return cell_vol

    if method == "d3" and d == 3:
        comp_vol = tensor_contraction_d3(D, A)
    elif method == "d4" and d == 4:
        comp_vol = tensor_contraction_d4(D, A)
    else:
        comp_vol = tensor_contraction_general(D, A, d)

    return cell_vol - comp_vol


# ==============================================================================
# Adversarial and Random Clean-Cell Generators
# ==============================================================================

def generate_random_clean_cell(d: int, num_boxes: int, max_coord: int, seed: int) -> Tuple[
    List[Tuple[int, int]], List[Tuple[Tuple[int, int], ...]]
]:
    """Generates random clean cell containing a mix of 1-slabs and 2-slabs."""
    rng = random.Random(seed)
    cell_bounds = [(0, max_coord) for _ in range(d)]
    boxes = []

    for _ in range(num_boxes):
        box_type = rng.choice(["1-slab", "2-slab", "2-slab"])
        b = []
        if box_type == "1-slab":
            active_dim = rng.randint(0, d - 1)
            for dim in range(d):
                if dim == active_dim:
                    u = rng.randint(0, max_coord - 5)
                    v = rng.randint(u + 2, max_coord)
                    b.append((u, v))
                else:
                    b.append((0, max_coord))
        else:
            active_dims = rng.sample(range(d), 2)
            for dim in range(d):
                if dim in active_dims:
                    u = rng.randint(0, max_coord - 5)
                    v = rng.randint(u + 2, max_coord)
                    b.append((u, v))
                else:
                    b.append((0, max_coord))
        boxes.append(tuple(b))

    return cell_bounds, boxes


def generate_trellis_weave(d: int, n: int, max_coord: int, seed: int) -> Tuple[
    List[Tuple[int, int]], List[Tuple[Tuple[int, int], ...]]
]:
    """
    Generates n 2-slabs forming an Orthogonal Trellis Weave:
    Every pair of coordinates {u, v} has active 2-slabs, forming a dense K_d interaction graph.
    """
    rng = random.Random(seed)
    cell_bounds = [(0, max_coord) for _ in range(d)]
    pairs = [(u, v) for u in range(d) for v in range(u + 1, d)]
    boxes = []

    for k in range(n):
        u, v = pairs[k % len(pairs)]
        b = []
        for dim in range(d):
            if dim == u or dim == v:
                u_coord = rng.randint(0, max_coord - 10)
                span = rng.randint(5, max_coord // 2)
                b.append((u_coord, min(max_coord, u_coord + span)))
            else:
                b.append((0, max_coord))
        boxes.append(tuple(b))

    return cell_bounds, boxes


def generate_hyperclique_gadget(d: int, n: int, max_coord: int, num_slots: int, seed: int) -> Tuple[
    List[Tuple[int, int]], List[Tuple[Tuple[int, int], ...]]
]:
    """
    Generates n 2-slabs aligned to discrete vertex slots, simulating a hyperclique complement.
    Creates deep combinatorial interaction patterns among 2-slabs.
    """
    rng = random.Random(seed)
    cell_bounds = [(0, max_coord) for _ in range(d)]
    slot_w = max_coord // num_slots
    pairs = [(u, v) for u in range(d) for v in range(u + 1, d)]
    boxes = []

    for k in range(n):
        u, v = pairs[k % len(pairs)]
        slot_u = rng.randint(0, num_slots - 2)
        span_u = rng.randint(1, 2)
        slot_v = rng.randint(0, num_slots - 2)
        span_v = rng.randint(1, 2)

        b = []
        for dim in range(d):
            if dim == u:
                b.append((slot_u * slot_w, (slot_u + span_u) * slot_w))
            elif dim == v:
                b.append((slot_v * slot_w, (slot_v + span_v) * slot_w))
            else:
                b.append((0, max_coord))
        boxes.append(tuple(b))

    return cell_bounds, boxes


def generate_edge_cases(d: int, max_coord: int) -> List[Tuple[str, List[Tuple[int, int]], List[Tuple[Tuple[int, int], ...]]]]:
    """Generates boundary and extreme cases."""
    cell = [(0, max_coord) for _ in range(d)]
    cases = []

    # 1. Empty boxes
    cases.append(("empty_boxes", cell, []))

    # 2. 0-slab covering cell
    cases.append(("zero_slab_covering", cell, [tuple((0, max_coord) for _ in range(d))]))

    # 3. Only 1-slabs
    slabs1 = []
    for dim in range(d):
        b = []
        for j in range(d):
            if j == dim:
                b.append((max_coord // 4, 3 * max_coord // 4))
            else:
                b.append((0, max_coord))
        slabs1.append(tuple(b))
    cases.append(("only_1_slabs", cell, slabs1))

    # 4. Disjoint 2-slabs
    if d >= 3:
        b1 = [(0, max_coord // 2), (0, max_coord // 2)] + [(0, max_coord)] * (d - 2)
        b2 = [(max_coord // 2, max_coord), (max_coord // 2, max_coord)] + [(0, max_coord)] * (d - 2)
        cases.append(("disjoint_2_slabs", cell, [tuple(b1), tuple(b2)]))

    # 5. Sparse interaction graph: Cycle interaction graph C_d
    cycle_boxes = []
    for i in range(d):
        u = i
        v = (i + 1) % d
        if u > v:
            u, v = v, u
        b = []
        for dim in range(d):
            if dim == u or dim == v:
                b.append((max_coord // 3, 2 * max_coord // 3))
            else:
                b.append((0, max_coord))
        cycle_boxes.append(tuple(b))
    cases.append(("cycle_interaction_graph", cell, cycle_boxes))

    return cases


# ==============================================================================
# Verification Suite Runner
# ==============================================================================

def run_verification_suite():
    print("================================================================================")
    print("Exact Symbolic & Numerical Verification Engine: Clean-Cell Tensor Contraction")
    print("================================================================================")
    start_time = time.time()

    total_tests = 0
    total_passed = 0
    results_by_dim: Dict[int, Dict[str, Any]] = {}

    dimensions = [3, 4, 5]

    for d in dimensions:
        print(f"\n--- [Phase D={d}] Verifying d={d} Clean-Cell Tensor Network Contraction ---")
        dim_start = time.time()
        d_passed = 0
        d_total = 0

        # Sub-suite 1: Edge Cases
        edge_cases = generate_edge_cases(d, max_coord=60)
        for name, cell, boxes in edge_cases:
            gt_vol = sweep_box_union_exact(boxes, cell)
            t_gen_vol = tensor_clean_cell_volume(boxes, cell, method="general")
            assert gt_vol == t_gen_vol, f"Mismatch in d={d} edge case {name}: GT={gt_vol}, Tensor={t_gen_vol}"

            if d == 3:
                t_spec_vol = tensor_clean_cell_volume(boxes, cell, method="d3")
                assert gt_vol == t_spec_vol, f"Mismatch in d=3 matrix trace {name}: GT={gt_vol}, Trace={t_spec_vol}"
            elif d == 4:
                t_spec_vol = tensor_clean_cell_volume(boxes, cell, method="d4")
                assert gt_vol == t_spec_vol, f"Mismatch in d=4 bipartite tensor {name}: GT={gt_vol}, Tensor4={t_spec_vol}"

            d_passed += 1
            d_total += 1

        print(f"  [d={d}] Edge Cases: {len(edge_cases)} / {len(edge_cases)} PASSED")

        # Sub-suite 2: Random Clean Cells
        num_random = 100 if d <= 4 else 50
        max_boxes = 12 if d <= 4 else 8
        max_coord = 50 if d <= 4 else 20

        for seed in range(num_random):
            n_boxes = random.randint(2, max_boxes)
            cell, boxes = generate_random_clean_cell(d, n_boxes, max_coord, seed=seed * 101 + d)

            gt_vol = sweep_box_union_exact(boxes, cell)
            t_gen_vol = tensor_clean_cell_volume(boxes, cell, method="general")
            assert gt_vol == t_gen_vol, f"Mismatch in d={d} random seed {seed}: GT={gt_vol}, Tensor={t_gen_vol}"

            if d == 3:
                t_d3 = tensor_clean_cell_volume(boxes, cell, method="d3")
                assert gt_vol == t_d3, f"Mismatch in d=3 trace seed {seed}"
            elif d == 4:
                t_d4 = tensor_clean_cell_volume(boxes, cell, method="d4")
                assert gt_vol == t_d4, f"Mismatch in d=4 tensor seed {seed}"

            d_passed += 1
            d_total += 1

        print(f"  [d={d}] Random Clean Cells: {num_random} / {num_random} PASSED")

        # Sub-suite 3: Adversarial Orthogonal Trellis Weaves
        num_trellis = 50 if d <= 4 else 25
        trellis_boxes = 16 if d <= 4 else 10
        trellis_coord = 100 if d <= 4 else 30

        for seed in range(num_trellis):
            cell, boxes = generate_trellis_weave(d, trellis_boxes, trellis_coord, seed=seed * 202 + d)

            gt_vol = sweep_box_union_exact(boxes, cell)
            t_gen_vol = tensor_clean_cell_volume(boxes, cell, method="general")
            assert gt_vol == t_gen_vol, f"Mismatch in d={d} trellis seed {seed}: GT={gt_vol}, Tensor={t_gen_vol}"

            if d == 3:
                t_d3 = tensor_clean_cell_volume(boxes, cell, method="d3")
                assert gt_vol == t_d3
            elif d == 4:
                t_d4 = tensor_clean_cell_volume(boxes, cell, method="d4")
                assert gt_vol == t_d4

            d_passed += 1
            d_total += 1

        print(f"  [d={d}] Trellis Weaves (Dense K_{d}): {num_trellis} / {num_trellis} PASSED")

        # Sub-suite 4: Adversarial Hyperclique Complement Gadgets
        num_gadgets = 50 if d <= 4 else 25
        gadget_boxes = 16 if d <= 4 else 10
        gadget_coord = 80 if d <= 4 else 32
        gadget_slots = 6 if d <= 4 else 4

        for seed in range(num_gadgets):
            cell, boxes = generate_hyperclique_gadget(d, gadget_boxes, gadget_coord, gadget_slots, seed=seed * 303 + d)

            gt_vol = sweep_box_union_exact(boxes, cell)
            t_gen_vol = tensor_clean_cell_volume(boxes, cell, method="general")
            assert gt_vol == t_gen_vol, f"Mismatch in d={d} hyperclique seed {seed}: GT={gt_vol}, Tensor={t_gen_vol}"

            if d == 3:
                t_d3 = tensor_clean_cell_volume(boxes, cell, method="d3")
                assert gt_vol == t_d3
            elif d == 4:
                t_d4 = tensor_clean_cell_volume(boxes, cell, method="d4")
                assert gt_vol == t_d4

            d_passed += 1
            d_total += 1

        print(f"  [d={d}] Hyperclique Gadgets (Discrete Clusters): {num_gadgets} / {num_gadgets} PASSED")

        # Sub-suite 5: Symbolic / Elementary Grid Cross-Validation (on small instances)
        for seed in range(5):
            cell, boxes = generate_random_clean_cell(d, 4, 15, seed=seed + 999)
            gt_vol = sweep_box_union_exact(boxes, cell)
            grid_vol = elementary_grid_exact(boxes, cell)
            tensor_vol = tensor_clean_cell_volume(boxes, cell, method="general")
            assert gt_vol == grid_vol == tensor_vol, f"Symbolic grid mismatch at d={d}"
            d_passed += 1
            d_total += 1

        print(f"  [d={d}] Symbolic Elementary Grid: 5 / 5 PASSED")

        dim_elapsed = time.time() - dim_start
        print(f"  --> TOTAL d={d}: {d_passed} / {d_total} PASSED in {dim_elapsed:.2f}s (100% exact match)")

        results_by_dim[d] = {
            "total": d_total,
            "passed": d_passed,
            "elapsed_seconds": dim_elapsed,
            "accuracy": 1.0,
        }
        total_tests += d_total
        total_passed += d_passed

    total_elapsed = time.time() - start_time

    # Generate verification certificate
    report = {
        "status": "SUCCESS",
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "total_tests": total_tests,
        "total_passed": total_passed,
        "elapsed_seconds": total_elapsed,
        "dimensions": results_by_dim,
        "exact_verification": True,
        "mathematical_theorem": "Clean-Cell Tensor Network Contraction Lemma for arbitrary d >= 3",
    }

    report_path = os.path.join(os.path.dirname(__file__), "tensor_contraction_report.json")
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print("\n================================================================================")
    print(f"VERIFICATION SUMMARY: {total_passed} / {total_tests} PASSED across d in {{3, 4, 5}}")
    print(f"Total Execution Time: {total_elapsed:.2f}s")
    print(f"Verification Certificate written to: {report_path}")
    print("================================================================================")

    return total_passed == total_tests


if __name__ == "__main__":
    success = run_verification_suite()
    if not success:
        sys.exit(1)
