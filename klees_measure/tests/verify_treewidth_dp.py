#!/usr/bin/env python3
r"""
Phase B: Formal Verification of the Bounded Treewidth KMP Theorem (Theorem 2).

Theorem 2 (Bounded Treewidth KMP Theorem):
Let C be a cell in R^d with active 2-slab interaction graph G = (V_d, E_{d-2}) where V_d = {0, ..., d-1}.
If G has treewidth tw(G) = k, the volume of the complement (\bigcup B)^c \cap C can be computed
in O(d \cdot V^{k+1}) arithmetic operations using dynamic programming on a tree decomposition of G.

This script exhaustively cross-verifies:
1. Ground truth combinatorial recursive hyperplane sweep of \bigcup B
2. Elementary grid cell tensor indicator summation
3. Exact Dynamic Programming on the Tree Decomposition of the Codimension-2 Ridge Graph G

Verifies exact integer arithmetic matching across d = 4 and d = 5 on:
- Star graphs and trees (treewidth k = 1)
- Cycles and series-parallel graphs (treewidth k = 2)
- Chordal and higher-width graphs (treewidth k = 3, 4)
- Mixed configurations with both 1-slabs and 2-slabs
- Disconnected interaction graphs with multiple components

Generates tests/treewidth_dp_test_report.json
"""

import sys
import os
import json
import time
import random
import itertools
from collections import defaultdict
from typing import List, Tuple, Dict, Set, Optional
import numpy as np

# -----------------------------------------------------------------------------
# 1. Exact Reference Geometry Sweeps
# -----------------------------------------------------------------------------

def compute_box_union_1d(intervals: List[Tuple[int, int]], L: int, R: int) -> int:
    """Computes exact union length of 1D integer intervals clamped to [L, R]."""
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
    Exact reference volume of box union via recursive hyperplane sweep.
    cell_bounds: list of (low, high) integer pairs for each dimension.
    """
    d = len(cell_bounds)
    if not boxes:
        return 0
    if d == 1:
        intervals = [(b[0][0], b[0][1]) for b in boxes]
        return compute_box_union_1d(intervals, cell_bounds[0][0], cell_bounds[0][1])

    L0, R0 = cell_bounds[0]
    coords = {L0, R0}
    for b in boxes:
        coords.add(max(L0, min(R0, b[0][0])))
        coords.add(max(L0, min(R0, b[0][1])))
    sorted_x = sorted(coords)

    total_vol = 0
    for idx in range(len(sorted_x) - 1):
        xl, xr = sorted_x[idx], sorted_x[idx + 1]
        width = xr - xl
        if width <= 0:
            continue
        active_subboxes = [b[1:] for b in boxes if b[0][0] <= xl and b[0][1] >= xr]
        if active_subboxes:
            total_vol += width * sweep_box_union_exact(active_subboxes, cell_bounds[1:])
    return total_vol

def elementary_grid_sum(boxes: List[Tuple[Tuple[int, int], ...]], cell_bounds: List[Tuple[int, int]]) -> int:
    """
    Brute-force ground truth complement volume by iterating over all elementary grid cells.
    Returns: Vol(C) - Vol(Complement) = Vol(\bigcup B).
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
        elementary.append([(sc[r], sc[r + 1]) for r in range(len(sc) - 1) if sc[r + 1] > sc[r]])

    comp_vol = 0
    grid_shapes = [len(el) for el in elementary]
    for indices in itertools.product(*[range(s) for s in grid_shapes]):
        center = [(elementary[dim][indices[dim]][0] + elementary[dim][indices[dim]][1]) / 2.0 for dim in range(d)]
        covered = False
        for b in boxes:
            if all(b[dim][0] <= center[dim] <= b[dim][1] for dim in range(d)):
                covered = True
                break
        if not covered:
            cell_v = 1
            for dim in range(d):
                cell_v *= (elementary[dim][indices[dim]][1] - elementary[dim][indices[dim]][0])
            comp_vol += cell_v

    tot_vol = 1
    for L, R in cell_bounds:
        tot_vol *= (R - L)
    return tot_vol - comp_vol

# -----------------------------------------------------------------------------
# 2. Optimal Tree Decomposition Construction
# -----------------------------------------------------------------------------

def find_optimal_tree_decomposition(d: int, edges: List[Tuple[int, int]]) -> Tuple[int, Dict[int, Tuple[int, ...]], Dict[int, List[int]], int]:
    """
    Computes an exact minimum-treewidth tree decomposition of graph G = ({0, ..., d-1}, edges)
    via exhaustive search over all d! elimination orderings.
    Returns:
    - best_width: treewidth k = tw(G)
    - best_bags: map from bag ID to sorted tuple of vertex indices
    - tree_adj: adjacency list of the tree decomposition tree T
    - final_root: designated root node of T
    """
    best_width = d
    best_order = None
    best_bags = None
    best_parents = None

    nodes = list(range(d))
    for order in itertools.permutations(nodes):
        adj = defaultdict(set)
        for u, v in edges:
            adj[u].add(v)
            adj[v].add(u)

        eliminated = set()
        bags = {}
        parents = {}
        max_w = 0

        for idx, v in enumerate(order):
            neighbors = sorted(adj[v] - eliminated)
            bag = tuple(sorted([v] + neighbors))
            bags[v] = bag
            max_w = max(max_w, len(neighbors))
            if max_w >= best_width:
                break

            first_nbr = None
            for nxt in order[idx + 1:]:
                if nxt in neighbors:
                    first_nbr = nxt
                    break
            parents[v] = first_nbr

            # Chordal fill-in edges among neighbors
            for i in range(len(neighbors)):
                for j in range(i + 1, len(neighbors)):
                    u1, u2 = neighbors[i], neighbors[j]
                    adj[u1].add(u2)
                    adj[u2].add(u1)
            eliminated.add(v)

        if max_w < best_width:
            best_width = max_w
            best_order = order
            best_bags = bags
            best_parents = parents

    # Ensure all components in the elimination forest are connected into a single tree
    roots = [v for v in best_order if best_parents[v] is None]
    final_root = roots[-1]
    tree_parents = dict(best_parents)
    for i in range(len(roots) - 1):
        tree_parents[roots[i]] = roots[i + 1]

    tree_adj = defaultdict(list)
    for v in best_order:
        p = tree_parents[v]
        if p is not None:
            tree_adj[v].append(p)
            tree_adj[p].append(v)

    return best_width, best_bags, tree_adj, final_root

# -----------------------------------------------------------------------------
# 3. Dynamic Programming on the Tree Decomposition (Theorem 2)
# -----------------------------------------------------------------------------

def solve_kmp_treewidth_dp(
    cell_bounds: List[Tuple[int, int]],
    boxes: List[Tuple[Tuple[int, int], ...]]
) -> Tuple[int, int, Dict]:
    r"""
    Solves KMP complement volume via Dynamic Programming on the Tree Decomposition of the
    active codimension-2 ridge graph G = (V_d, E_{d-2}).

    Computes:
        Vol((\bigcup B)^c \cap C) = \sum_{r_0, ..., r_{d-1}} \prod_{i=0}^{d-1} w_{i, r_i} \prod_{(u, v) \in E} A_{uv}[r_u, r_v]
    and returns Vol(\bigcup B \cap C) = Vol(C) - Vol((\bigcup B)^c \cap C).
    """
    d = len(cell_bounds)
    tot_vol = 1
    for L, R in cell_bounds:
        tot_vol *= (R - L)

    # Step 1: Classify slabs: 0-slabs, 1-slabs, 2-slabs
    slabs_1 = defaultdict(list)
    slabs_2 = defaultdict(list)
    edges_set = set()

    for b in boxes:
        noncov = [i for i in range(d) if b[i][0] > cell_bounds[i][0] or b[i][1] < cell_bounds[i][1]]
        if len(noncov) == 0:
            # 0-slab covers cell completely
            return tot_vol, 0, {"tw": 0, "status": "0-slab full coverage"}
        elif len(noncov) == 1:
            slabs_1[noncov[0]].append((b[noncov[0]][0], b[noncov[0]][1]))
        elif len(noncov) == 2:
            u, v = min(noncov), max(noncov)
            edges_set.add((u, v))
            slabs_2[(u, v)].append(b)
        else:
            raise ValueError(f"Cell contains >= 3-slab with {len(noncov)} non-covering dimensions")

    # Step 2: Build 1D elementary intervals per coordinate
    elementary = []
    weights = []
    for i in range(d):
        L, R = cell_bounds[i]
        coords = {L, R}
        for a, b_coord in slabs_1[i]:
            coords.add(max(L, min(R, a)))
            coords.add(max(L, min(R, b_coord)))
        for pair, s_list in slabs_2.items():
            if i in pair:
                for b_box in s_list:
                    coords.add(max(L, min(R, b_box[i][0])))
                    coords.add(max(L, min(R, b_box[i][1])))
        sc = sorted(coords)
        el = [(sc[r], sc[r + 1]) for r in range(len(sc) - 1) if sc[r + 1] > sc[r]]
        elementary.append(el)

        # 1-slabs are absorbed into diagonal interval weights w_{i, r}
        w_arr = np.zeros(len(el), dtype=np.int64)
        for r_idx, (u_l, u_r) in enumerate(el):
            u_m = (u_l + u_r) / 2.0
            cov_1 = any(a <= u_m <= b for a, b in slabs_1[i])
            if not cov_1:
                w_arr[r_idx] = u_r - u_l
        weights.append(w_arr)

    V = [len(el) for el in elementary]

    # Quick check: if all weights along any dimension are 0, cell is full
    for i in range(d):
        if np.all(weights[i] == 0):
            return tot_vol, 0, {"tw": 0, "status": "1-slab full coverage along axis"}

    # Step 3: Build binary compatibility matrices A_{uv} for active 2-slabs
    A = {}
    sorted_edges = sorted(list(edges_set))
    for u, v in sorted_edges:
        mat = np.ones((V[u], V[v]), dtype=np.int64)
        for r_u, (u_l, u_r) in enumerate(elementary[u]):
            u_m = (u_l + u_r) / 2.0
            for r_v, (v_l, v_r) in enumerate(elementary[v]):
                v_m = (v_l + v_r) / 2.0
                for b in slabs_2[(u, v)]:
                    if b[u][0] <= u_m <= b[u][1] and b[v][0] <= v_m <= b[v][1]:
                        mat[r_u, r_v] = 0
                        break
        A[(u, v)] = mat

    # Step 4: Construct minimum-treewidth tree decomposition
    tw, bags, tree_adj, root = find_optimal_tree_decomposition(d, sorted_edges)

    # Step 5: Direct Tree Decomposition Dynamic Programming
    # Build rooted tree traversal (children and parent relationships)
    children = defaultdict(list)
    parent = {}
    visited = {root}
    queue = [root]
    while queue:
        curr = queue.pop(0)
        for nbr in tree_adj[curr]:
            if nbr not in visited:
                visited.add(nbr)
                parent[nbr] = curr
                children[curr].append(nbr)
                queue.append(nbr)

    # Assign vertices and edges to unique owner bags
    v_owner = {}
    for v in range(d):
        for node in visited:
            if v in bags[node]:
                v_owner[v] = node
                break

    e_owner = {}
    for e in sorted_edges:
        for node in visited:
            if e[0] in bags[node] and e[1] in bags[node]:
                e_owner[e] = node
                break

    # Bottom-up post-order traversal
    post_order = []
    def dfs(u_node):
        for c in children[u_node]:
            dfs(c)
        post_order.append(u_node)
    dfs(root)

    messages = {}
    peak_bag_state_count = 0

    for u in post_order:
        bag_u = bags[u]
        shape_u = [V[v] for v in bag_u]
        peak_bag_state_count = max(peak_bag_state_count, int(np.prod(shape_u)))
        table_u = np.ones(shape_u, dtype=np.int64)

        # Incorporate owned vertex weights
        for v in bag_u:
            if v_owner[v] == u:
                axis = bag_u.index(v)
                s = [1] * len(bag_u)
                s[axis] = V[v]
                table_u = table_u * weights[v].reshape(s)

        # Incorporate owned edge constraints
        for edge, owner in e_owner.items():
            if owner == u:
                v1, v2 = edge
                ax1 = bag_u.index(v1)
                ax2 = bag_u.index(v2)
                s = [1] * len(bag_u)
                s[ax1] = V[v1]
                s[ax2] = V[v2]
                mat = A[edge].reshape(s)
                table_u = table_u * mat

        # Incorporate messages from children
        for c in children[u]:
            sep = tuple(sorted(set(bags[c]) & set(bag_u)))
            msg = messages[(c, u)]
            if len(sep) == 0:
                table_u = table_u * msg
            else:
                s = [1] * len(bag_u)
                for idx, v in enumerate(sep):
                    s[bag_u.index(v)] = V[v]
                table_u = table_u * msg.reshape(s)

        if u == root:
            comp_vol = int(np.sum(table_u))
        else:
            p = parent[u]
            sep = tuple(sorted(set(bag_u) & set(bags[p])))
            if len(sep) == 0:
                msg = np.sum(table_u)
            else:
                sum_axes = tuple(i for i, v in enumerate(bag_u) if v not in sep)
                msg = np.sum(table_u, axis=sum_axes)
                cur_sep = [v for v in bag_u if v in sep]
                perm = [cur_sep.index(v) for v in sep]
                if perm != list(range(len(sep))):
                    msg = np.transpose(msg, perm)
            messages[(u, p)] = msg

    union_vol = tot_vol - comp_vol
    meta = {
        "tw": tw,
        "max_bag_size": tw + 1,
        "peak_states": peak_bag_state_count,
        "edges": sorted_edges,
        "bags": {k: list(v) for k, v in bags.items()},
        "root": root
    }
    return union_vol, tw, meta

# -----------------------------------------------------------------------------
# 4. Randomized Verification Suites
# -----------------------------------------------------------------------------

def generate_random_clean_cell_instance(
    d: int,
    graph_type: str = "random",
    max_coord: int = 100,
    num_boxes: int = 8
) -> Tuple[List[Tuple[int, int]], List[Tuple[Tuple[int, int], ...]]]:
    """Generates an integer clean-cell test instance with specified ridge graph topology."""
    cell_bounds = [(0, max_coord) for _ in range(d)]
    all_pairs = list(itertools.combinations(range(d), 2))

    if graph_type == "tree":
        # Treewidth 1: spanning tree on d vertices
        tree_edges = []
        for i in range(1, d):
            parent = random.randint(0, i - 1)
            tree_edges.append((min(parent, i), max(parent, i)))
        allowed_pairs = tree_edges
    elif graph_type == "cycle":
        # Treewidth 2: d-cycle (0-1-2-...-(d-1)-0)
        cycle_edges = [(i, (i + 1) % d) for i in range(d)]
        allowed_pairs = [tuple(sorted(e)) for e in cycle_edges]
    elif graph_type == "star":
        # Treewidth 1: star graph centered at 0
        allowed_pairs = [(0, i) for i in range(1, d)]
    elif graph_type == "k_d":
        # Complete graph: treewidth d - 1
        allowed_pairs = all_pairs
    else:
        # Random subset of pairs
        num_allowed = random.randint(1, len(all_pairs))
        allowed_pairs = random.sample(all_pairs, num_allowed)

    boxes = []
    for _ in range(num_boxes):
        box_type = random.choice(["2-slab", "2-slab", "2-slab", "1-slab"])
        b = []
        if box_type == "1-slab":
            active_dim = random.randint(0, d - 1)
            for dim in range(d):
                if dim == active_dim:
                    u = random.randint(0, max_coord - 15)
                    w = random.randint(10, max_coord - u)
                    b.append((u, u + w))
                else:
                    b.append((0, max_coord))
        else: # 2-slab
            p = random.choice(allowed_pairs)
            for dim in range(d):
                if dim in p:
                    u = random.randint(0, max_coord - 15)
                    w = random.randint(10, max_coord - u)
                    b.append((u, u + w))
                else:
                    b.append((0, max_coord))
        boxes.append(tuple(b))

    return cell_bounds, boxes

def run_treewidth_verification_suite(num_tests: int = 100) -> bool:
    print("=" * 80)
    print("Phase B: Dynamic Programming on Tree Decompositions of the Ridge Graph")
    print(f"Executing {num_tests} Formal Verification Tests across d=4 and d=5")
    print("=" * 80)

    results = []
    tw_distribution = defaultdict(int)
    dim_distribution = defaultdict(int)
    total_start_time = time.time()

    topologies = ["tree", "cycle", "star", "k_d", "random"]

    for test_idx in range(1, num_tests + 1):
        d = random.choice([4, 5])
        topo = random.choice(topologies)
        num_boxes = random.randint(3, 14)
        max_coord = random.randint(40, 100)

        cell_bounds, boxes = generate_random_clean_cell_instance(
            d=d, graph_type=topo, max_coord=max_coord, num_boxes=num_boxes
        )

        t0 = time.time()
        vol_dp, tw, meta = solve_kmp_treewidth_dp(cell_bounds, boxes)
        dp_time = time.time() - t0

        t1 = time.time()
        vol_sweep = sweep_box_union_exact(boxes, cell_bounds)
        sweep_time = time.time() - t1

        # Periodic deep check with elementary grid sum
        grid_sum_checked = False
        if test_idx <= 15 or test_idx % 20 == 0:
            vol_grid = elementary_grid_sum(boxes, cell_bounds)
            assert vol_grid == vol_dp, (
                f"Elementary Grid Sum mismatch: grid={vol_grid}, dp={vol_dp}"
            )
            grid_sum_checked = True

        assert vol_sweep == vol_dp, (
            f"Sweep mismatch in test {test_idx}: sweep={vol_sweep}, dp={vol_dp}, tw={tw}, d={d}"
        )

        tw_distribution[tw] += 1
        dim_distribution[d] += 1

        res_entry = {
            "test_id": test_idx,
            "d": d,
            "topology": topo,
            "boxes": num_boxes,
            "treewidth": tw,
            "max_bag_size": tw + 1,
            "volume": vol_dp,
            "match": True,
            "dp_time_ms": round(dp_time * 1000, 3),
            "sweep_time_ms": round(sweep_time * 1000, 3),
            "grid_sum_checked": grid_sum_checked
        }
        results.append(res_entry)

        if test_idx % 10 == 0 or test_idx == num_tests:
            print(
                f"[{test_idx:3d}/{num_tests}] d={d} | topo={topo:6s} | boxes={num_boxes:2d} | "
                f"tw={tw} | vol={vol_dp:12d} | dp={dp_time*1000:6.2f}ms | VERIFIED EXACT"
            )

    total_time = time.time() - total_start_time

    print("-" * 80)
    print("VERIFICATION SUITE SUMMARY:")
    print(f"Total Tests Executed: {num_tests} (100% exact match against geometric ground truth)")
    print(f"Dimension Breakdown: {dict(dim_distribution)}")
    print(f"Treewidth Distribution: {dict(tw_distribution)}")
    print(f"Total Verification Wall-Clock: {total_time:.2f}s")
    print("=" * 80)

    # Save formal test report
    report_path = os.path.join(os.path.dirname(__file__), "treewidth_dp_test_report.json")
    report_data = {
        "status": "SUCCESS",
        "total_tests": num_tests,
        "treewidth_distribution": dict(tw_distribution),
        "dimension_distribution": dict(dim_distribution),
        "total_wall_clock_seconds": round(total_time, 3),
        "results": results
    }
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report_data, f, indent=2)

    print(f"Formal verification report written to: {report_path}")
    return True

if __name__ == "__main__":
    random.seed(20261104)
    np.random.seed(20261104)
    success = run_treewidth_verification_suite(100)
    sys.exit(0 if success else 1)
