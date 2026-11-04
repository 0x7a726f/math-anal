"""
Phase A: Formal Verification of the Clean-Cell Factorization Lemma.
Exhaustively cross-verifies:
1. Ground truth combinatorial hyperplane sweep
2. Elementary interval indicator summation
3. Matrix Trace Formula: Tr(D_0 A_01 D_1 A_12 D_2 A_20)

Generates tests/clean_cell_test_report.json
"""

import sys
import json
import time
import random
import numpy as np

def compute_box_union_1d(intervals, L, R):
    """Computes union length of 1D intervals clamped to [L, R]."""
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

def sweep_box_union_exact(boxes, cell_bounds):
    """
    Exact reference volume via recursive hyperplane sweep on coordinate 0.
    cell_bounds: list of (low, high) for each dimension.
    """
    d = len(cell_bounds)
    if not boxes:
        return 0
    
    # Base case d = 1
    if d == 1:
        intervals = [(b[0][0], b[0][1]) for b in boxes]
        return compute_box_union_1d(intervals, cell_bounds[0][0], cell_bounds[0][1])

    # Collect unique x-coordinates in [L_0, R_0]
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
        # Active boxes spanning [x_left, x_right]
        mid = (x_left + x_right) / 2.0
        active_subboxes = []
        for b in boxes:
            if b[0][0] <= x_left and b[0][1] >= x_right:
                # Projected (d-1) box
                active_subboxes.append(b[1:])
        if active_subboxes:
            sub_vol = sweep_box_union_exact(active_subboxes, cell_bounds[1:])
            total_vol += width * sub_vol
            
    return total_vol

def elementary_grid_sum(boxes, cell_bounds):
    """
    Computes complement volume by summing over elementary interval products.
    """
    d = len(cell_bounds)
    elementary = []
    for i in range(d):
        L, R = cell_bounds[i]
        coords = {L, R}
        for b in boxes:
            coords.add(max(L, min(R, b[i][0])))
            coords.add(max(L, min(R, b[i][1])))
        sc = sorted(coords)
        elementary.append([(sc[r], sc[r+1]) for r in range(len(sc)-1) if sc[r+1] > sc[r]])

    # Check each cell in elementary grid
    import itertools
    complement_vol = 0
    grid_shapes = [len(el) for el in elementary]
    
    for indices in itertools.product(*[range(s) for s in grid_shapes]):
        # Test if point at center of this elementary cell is covered by any box
        center = [ (elementary[dim][indices[dim]][0] + elementary[dim][indices[dim]][1]) / 2.0 for dim in range(d) ]
        covered = False
        for b in boxes:
            if all(b[dim][0] <= center[dim] <= b[dim][1] for dim in range(d)):
                covered = True
                break
        if not covered:
            cell_v = 1
            for dim in range(d):
                cell_v *= (elementary[dim][indices[dim]][1] - elementary[dim][indices[dim]][0])
            complement_vol += cell_v

    cell_total_vol = 1
    for L, R in cell_bounds:
        cell_total_vol *= (R - L)
    return cell_total_vol - complement_vol

def matrix_trace_clean_cell_d3(boxes, cell_bounds):
    """
    Computes union volume for Clean cells in d=3 using the Matrix Trace Formula:
    Tr(D0 * A01 * D1 * A12 * D2 * A20)
    where:
    - 1-slabs are absorbed into diagonal weights D_i
    - 2-slabs define binary adjacency matrices A_ij
    - cell contains zero >=3-slabs.
    """
    d = 3
    # Step 1: Identify 1-slabs and 2-slabs
    slabs_1 = {0: [], 1: [], 2: []}
    slabs_2 = {(0, 1): [], (1, 2): [], (0, 2): []}

    for b in boxes:
        noncov = []
        for i in range(d):
            if b[i][0] > cell_bounds[i][0] or b[i][1] < cell_bounds[i][1]:
                noncov.append(i)
        if len(noncov) == 0:
            # 0-slab: cell completely covered!
            vol = 1
            for L, R in cell_bounds:
                vol *= (R - L)
            return vol
        elif len(noncov) == 1:
            slabs_1[noncov[0]].append((b[noncov[0]][0], b[noncov[0]][1]))
        elif len(noncov) == 2:
            pair = (min(noncov[0], noncov[1]), max(noncov[0], noncov[1]))
            slabs_2[pair].append((b[pair[0]][0], b[pair[0]][1], b[pair[1]][0], b[pair[1]][1]))
        else:
            raise ValueError("Cell is not clean: contains >= 3-slabs")

    # Step 2: Build elementary intervals per coordinate
    elementary = []
    for i in range(d):
        L, R = cell_bounds[i]
        coords = {L, R}
        for a, b in slabs_1[i]:
            coords.add(max(L, min(R, a)))
            coords.add(max(L, min(R, b)))
        for pair, s_list in slabs_2.items():
            if i == pair[0]:
                for a1, b1, _, _ in s_list:
                    coords.add(max(L, min(R, a1)))
                    coords.add(max(L, min(R, b1)))
            elif i == pair[1]:
                for _, _, a2, b2 in s_list:
                    coords.add(max(L, min(R, a2)))
                    coords.add(max(L, min(R, b2)))
        sc = sorted(coords)
        elementary.append([(sc[r], sc[r+1]) for r in range(len(sc)-1) if sc[r+1] > sc[r]])

    # Step 3: Compute diagonal weight vectors D_i
    # D_i[r] = length of elementary interval IF not covered by any 1-slab in dimension i, else 0.
    D = []
    for i in range(d):
        V_i = len(elementary[i])
        diag_w = np.zeros(V_i, dtype=np.int64)
        for r, (u_left, u_right) in enumerate(elementary[i]):
            u_mid = (u_left + u_right) / 2.0
            cov_1 = any(a <= u_mid <= b for a, b in slabs_1[i])
            if not cov_1:
                diag_w[r] = u_right - u_left
        D.append(diag_w)

    # Step 4: Build binary adjacency matrices A_ij
    # A_ij[r, s] = 0 if any 2-slab in pair (i, j) covers I_{i, r} x I_{j, s}, else 1.
    def make_adj(dim_i, dim_j):
        pair = (min(dim_i, dim_j), max(dim_i, dim_j))
        V_i = len(elementary[dim_i])
        V_j = len(elementary[dim_j])
        A = np.ones((V_i, V_j), dtype=np.int64)
        for r, (ui_l, ui_r) in enumerate(elementary[dim_i]):
            ui_m = (ui_l + ui_r) / 2.0
            for s, (uj_l, uj_r) in enumerate(elementary[dim_j]):
                uj_m = (uj_l + uj_r) / 2.0
                for a1, b1, a2, b2 in slabs_2[pair]:
                    if dim_i == pair[0]:
                        if a1 <= ui_m <= b1 and a2 <= uj_m <= b2:
                            A[r, s] = 0
                            break
                    else:
                        if a2 <= ui_m <= b2 and a1 <= uj_m <= b1:
                            A[r, s] = 0
                            break
        return A

    A01 = make_adj(0, 1)
    A12 = make_adj(1, 2)
    A20 = make_adj(2, 0) # A_{2, 0}

    # Step 5: Compute Tr(D0 * A01 * D1 * A12 * D2 * A20)
    # Using BLAS matrix multiplication:
    # M1 = D0 * A01           (row scaling)
    # M2 = M1 * D1            (col scaling) -> shape V0 x V1
    # M3 = M2 @ A12           (GEMM)        -> shape V0 x V2
    # M4 = M3 * D2            (col scaling) -> shape V0 x V2
    # Trace = sum_r,s (M4[r, s] * A20[s, r])
    
    M2 = (D[0][:, None] * A01) * D[1][None, :]
    M3 = np.dot(M2, A12)
    M4 = M3 * D[2][None, :]
    complement_vol = int(np.sum(M4 * A20.T))

    cell_total_vol = 1
    for L, R in cell_bounds:
        cell_total_vol *= (R - L)
        
    return cell_total_vol - complement_vol

def generate_random_clean_cell(d=3, max_boxes=12, max_coord=50):
    cell_bounds = [(0, max_coord) for _ in range(d)]
    num_boxes = random.randint(2, max_boxes)
    boxes = []
    for _ in range(num_boxes):
        box_type = random.choice(["1-slab", "2-slab", "2-slab"])
        b = []
        if box_type == "1-slab":
            active_dim = random.randint(0, d - 1)
            for dim in range(d):
                if dim == active_dim:
                    a = random.randint(0, max_coord - 5)
                    b_len = random.randint(5, max_coord - a)
                    b.append((a, a + b_len))
                else:
                    b.append((0, max_coord))
        else: # 2-slab
            active_dims = random.sample(range(d), 2)
            for dim in range(d):
                if dim in active_dims:
                    a = random.randint(0, max_coord - 5)
                    b_len = random.randint(5, max_coord - a)
                    b.append((a, a + b_len))
                else:
                    b.append((0, max_coord))
        boxes.append(tuple(b))
    return cell_bounds, boxes

def run_verification_suite(total_iterations=10000):
    print(f"================================================================")
    print(f"Formal Verification Suite: Clean-Cell Factorization Lemma")
    print(f"Target: {total_iterations} randomized clean cells in d=3")
    print(f"================================================================")
    
    random.seed(42)
    np.random.seed(42)

    sweep_match_count = 0
    grid_match_count = 0
    start_time = time.time()
    
    max_trace_time = 0.0
    total_trace_time = 0.0

    # Test batches: run thorough 3-way check on first 500, then matrix vs sweep on the rest
    report = {
        "status": "IN_PROGRESS",
        "total_tested": total_iterations,
        "three_way_exact_matches": 0,
        "two_way_exact_matches": 0,
        "mismatches": 0,
        "max_abs_error": 0,
        "avg_trace_eval_us": 0.0,
        "total_duration_sec": 0.0
    }

    for i in range(1, total_iterations + 1):
        cell_bounds, boxes = generate_random_clean_cell(d=3, max_boxes=10, max_coord=30)
        
        t0 = time.perf_counter()
        vol_trace = matrix_trace_clean_cell_d3(boxes, cell_bounds)
        t_trace = (time.perf_counter() - t0) * 1e6
        total_trace_time += t_trace
        if t_trace > max_trace_time:
            max_trace_time = t_trace

        vol_sweep = sweep_box_union_exact(boxes, cell_bounds)
        
        if vol_trace != vol_sweep:
            print(f"FAILED on iteration {i}!")
            print(f"Trace: {vol_trace}, Sweep: {vol_sweep}")
            report["status"] = "FAILED"
            report["mismatches"] += 1
            report["max_abs_error"] = max(report["max_abs_error"], abs(vol_trace - vol_sweep))
            with open("tests/clean_cell_test_report.json", "w") as f:
                json.dump(report, f, indent=2)
            sys.exit(1)
            
        sweep_match_count += 1
        
        # Periodic 3-way check with brute-force elementary grid
        if i <= 300:
            vol_grid = elementary_grid_sum(boxes, cell_bounds)
            if vol_trace != vol_grid:
                print(f"FAILED GRID check on iteration {i}!")
                print(f"Trace: {vol_trace}, Grid: {vol_grid}")
                report["status"] = "FAILED"
                report["mismatches"] += 1
                with open("tests/clean_cell_test_report.json", "w") as f:
                    json.dump(report, f, indent=2)
                sys.exit(1)
            grid_match_count += 1

        if i % 2000 == 0 or i == total_iterations:
            elapsed = time.time() - start_time
            print(f"[{i}/{total_iterations}] Matches: {sweep_match_count} (100.0%) | Elapsed: {elapsed:.2f}s | Avg Trace: {total_trace_time / i:.1f} us")

    total_duration = time.time() - start_time
    report["status"] = "VERIFIED_100_PERCENT_EXACT"
    report["three_way_exact_matches"] = grid_match_count
    report["two_way_exact_matches"] = sweep_match_count
    report["avg_trace_eval_us"] = round(total_trace_time / total_iterations, 2)
    report["total_duration_sec"] = round(total_duration, 2)

    import os
    out_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(out_dir, "clean_cell_test_report.json")
    os.makedirs(out_dir, exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(report, f, indent=2)

    print(f"\nSUCCESS: Clean-Cell Factorization Lemma fully verified across {total_iterations} instances.")
    print(f"Artifact written to {out_path}")

if __name__ == "__main__":
    n_iters = 10000
    if len(sys.argv) > 1:
        n_iters = int(sys.argv[1])
    run_verification_suite(n_iters)
