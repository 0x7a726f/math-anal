"""
Klee's Measure Problem: Complete Algorithmic Suite Across Dimensions
====================================================================
Implements exact deterministic algorithms for computing the d-dimensional
Lebesgue measure (hypervolume) of the union of n axis-aligned boxes in R^d:

1. klee_1d:
   O(n log n) interval sorting and sweep for d = 1 (Klee 1977).

2. klee_2d_bentley:
   O(n log n) sweep-line with an explicit Coordinate-Compressed Segment Tree
   for d = 2 (Bentley 1977).

3. klee_grid_exact:
   Exact coordinate-compressed d-dimensional arrangement grid evaluation
   for ground-truth verification on small n.

4. klee_sweep_nd:
   O(n^{d-1} log n) recursive hyperplane sweep reducing dimension d -> d-1
   down to the O(n log n) 2D base case (Bentley 1977).

5. klee_chan_sdc:
   O(n^{d/2}) Timothy Chan's (FOCS 2013) "Simplify, Divide, and Conquer" (SDC)
   algorithm featuring:
   - Exact 0-slab short-circuiting,
   - Iterative 1-slab elimination via piecewise-linear order-preserving
     coordinate compression phi_i,
   - Weighted median hyperplane splitting on codimension-2 ridges.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Tuple, Optional
import math

EPS = 1e-12

# A Box in R^d is represented as a tuple of d pairs: ((a_0, b_0), ..., (a_{d-1}, b_{d-1}))
Box = Tuple[Tuple[float, float], ...]


def validate_boxes(boxes: List[Box]) -> Tuple[List[Box], int]:
    """Filter out degenerate/empty boxes and return (valid_boxes, dimension)."""
    if not boxes:
        return [], 0
    d = len(boxes[0])
    valid = []
    for b in boxes:
        if len(b) != d:
            raise ValueError(f"Inconsistent box dimension: expected {d}, got {len(b)}")
        if all(b[i][1] - b[i][0] > EPS for i in range(d)):
            valid.append(tuple((float(b[i][0]), float(b[i][1])) for i in range(d)))
    return valid, d


# ============================================================================
# 1. Dimension d = 1: O(n log n) Interval Union
# ============================================================================

def klee_1d(intervals: List[Tuple[float, float]]) -> float:
    """
    Compute the 1D measure of the union of n intervals [a_i, b_i] in O(n log n) time.
    """
    valid = [(a, b) for a, b in intervals if b - a > EPS]
    if not valid:
        return 0.0
    valid.sort(key=lambda x: (x[0], x[1]))
    total = 0.0
    cur_a, cur_b = valid[0]
    for a, b in valid[1:]:
        if a <= cur_b + EPS:
            if b > cur_b:
                cur_b = b
        else:
            total += cur_b - cur_a
            cur_a, cur_b = a, b
    total += cur_b - cur_a
    return total


def union_1d_disjoint(intervals: List[Tuple[float, float]]) -> List[Tuple[float, float]]:
    """
    Return the sorted list of disjoint closed intervals forming the union of `intervals`.
    """
    valid = [(a, b) for a, b in intervals if b - a > EPS]
    if not valid:
        return []
    valid.sort(key=lambda x: (x[0], x[1]))
    merged = []
    cur_a, cur_b = valid[0]
    for a, b in valid[1:]:
        if a <= cur_b + EPS:
            if b > cur_b:
                cur_b = b
        else:
            merged.append((cur_a, cur_b))
            cur_a, cur_b = a, b
    merged.append((cur_a, cur_b))
    return merged


# ============================================================================
# 2. Dimension d = 2: Bentley's O(n log n) Sweep-Line + Segment Tree
# ============================================================================

class SegmentTree1D:
    """
    Coordinate-compressed segment tree over elementary intervals
    [y_0, y_1], [y_1, y_2], ..., [y_{m-1}, y_m].
    Supports O(log m) range cover/uncover updates and O(1) active measure query.
    """

    def __init__(self, coords: List[float]):
        self.coords = coords
        self.m = len(coords) - 1  # number of elementary intervals
        self.count = [0] * (4 * max(1, self.m))
        self.length = [0.0] * (4 * max(1, self.m))

    def _pull(self, node: int, l: int, r: int) -> None:
        if self.count[node] > 0:
            self.length[node] = self.coords[r + 1] - self.coords[l]
        elif l == r:
            self.length[node] = 0.0
        else:
            self.length[node] = self.length[2 * node] + self.length[2 * node + 1]

    def update(self, ql: int, qr: int, delta: int) -> None:
        if self.m <= 0 or ql > qr:
            return
        self._update(1, 0, self.m - 1, ql, qr, delta)

    def _update(self, node: int, l: int, r: int, ql: int, qr: int, delta: int) -> None:
        if ql <= l and r <= qr:
            self.count[node] += delta
            self._pull(node, l, r)
            return
        mid = (l + r) // 2
        if ql <= mid:
            self._update(2 * node, l, mid, ql, qr, delta)
        if qr > mid:
            self._update(2 * node + 1, mid + 1, r, ql, qr, delta)
        self._pull(node, l, r)

    @property
    def covered_measure(self) -> float:
        return self.length[1] if self.m > 0 else 0.0


def klee_2d_bentley(boxes: List[Box]) -> float:
    """
    Compute the 2D area of the union of n axis-aligned rectangles in O(n log n) time
    using Bentley's sweep-line algorithm with a coordinate-compressed segment tree.
    """
    valid, d = validate_boxes(boxes)
    if not valid:
        return 0.0
    if d != 2:
        raise ValueError(f"klee_2d_bentley requires d=2, got d={d}")

    y_coords = sorted({y for b in valid for y in (b[1][0], b[1][1])})
    if len(y_coords) < 2:
        return 0.0
    y_index = {y: i for i, y in enumerate(y_coords)}

    # Events: (x, delta, y_low_idx, y_high_idx)
    events = []
    for (x0, x1), (y0, y1) in valid:
        yi0 = y_index[y0]
        yi1 = y_index[y1]
        if yi0 < yi1:
            events.append((x0, +1, yi0, yi1 - 1))
            events.append((x1, -1, yi0, yi1 - 1))

    events.sort(key=lambda e: (e[0], -e[1]))
    seg_tree = SegmentTree1D(y_coords)

    area = 0.0
    prev_x = events[0][0]
    for x, delta, ql, qr in events:
        dx = x - prev_x
        if dx > EPS:
            area += dx * seg_tree.covered_measure
            prev_x = x
        seg_tree.update(ql, qr, delta)

    return area


# ============================================================================
# 3. Exact Coordinate-Compressed Arrangement Grid Baseline (Any d)
# ============================================================================

def klee_grid_exact(boxes: List[Box]) -> float:
    """
    Ground-truth exact volume via coordinate compression in all d dimensions.
    Runs in O(n^d) time; intended for rigorous verification on small instances.
    """
    valid, d = validate_boxes(boxes)
    if not valid:
        return 0.0
    if d == 1:
        return klee_1d([b[0] for b in valid])
    if d == 2:
        return klee_2d_bentley(valid)

    coords = [sorted({b[i][0] for b in valid} | {b[i][1] for b in valid}) for i in range(d)]
    if any(len(c) < 2 for c in coords):
        return 0.0

    idx_maps = [{v: idx for idx, v in enumerate(coords[i])} for i in range(d)]
    dims = [len(coords[i]) - 1 for i in range(d)]

    # Represent each box by its integer index ranges in each dimension
    int_boxes = [
        tuple((idx_maps[i][b[i][0]], idx_maps[i][b[i][1]]) for i in range(d))
        for b in valid
    ]

    # Recursively sweep over the first d-1 dimensions and do 1D bitmask/interval union on the last
    def recurse(dim: int, active_boxes: List[Tuple[Tuple[int, int], ...]]) -> float:
        if not active_boxes:
            return 0.0
        if dim == d - 1:
            # 1D union on the last dimension
            covered = [False] * dims[dim]
            for b in active_boxes:
                for k in range(b[dim][0], b[dim][1]):
                    covered[k] = True
            return sum(
                coords[dim][k + 1] - coords[dim][k]
                for k in range(dims[dim])
                if covered[k]
            )
        total = 0.0
        for k in range(dims[dim]):
            length_k = coords[dim][k + 1] - coords[dim][k]
            sub_boxes = [b for b in active_boxes if b[dim][0] <= k and k < b[dim][1]]
            if sub_boxes:
                total += length_k * recurse(dim + 1, sub_boxes)
        return total

    return recurse(0, int_boxes)


# ============================================================================
# 4. Bentley's Recursive Hyperplane Sweep: O(n^{d-1} log n)
# ============================================================================

def klee_sweep_nd(boxes: List[Box]) -> float:
    """
    Compute the d-dimensional measure of the union of boxes by recursively
    sweeping a hyperplane along dimension d-1 and invoking the (d-1)-dimensional
    algorithm at each active slab, terminating at klee_2d_bentley for d = 2.
    Time complexity: O(n^{d-1} log n) for d >= 2.
    """
    valid, d = validate_boxes(boxes)
    if not valid:
        return 0.0
    if d == 1:
        return klee_1d([b[0] for b in valid])
    if d == 2:
        return klee_2d_bentley(valid)

    z_coords = sorted({z for b in valid for z in (b[d - 1][0], b[d - 1][1])})
    total_vol = 0.0
    for k in range(len(z_coords) - 1):
        z0, z1 = z_coords[k], z_coords[k + 1]
        dz = z1 - z0
        if dz <= EPS:
            continue
        # Collect (d-1)-dimensional projections of boxes spanning [z0, z1]
        slice_boxes = [
            b[: d - 1]
            for b in valid
            if b[d - 1][0] <= z0 + EPS and b[d - 1][1] >= z1 - EPS
        ]
        if slice_boxes:
            total_vol += dz * klee_sweep_nd(slice_boxes)
    return total_vol


# ============================================================================
# 5. Timothy Chan's (FOCS 2013) O(n^{d/2}) Simplify, Divide, and Conquer (SDC)
# ============================================================================

@dataclass
class ChanSDCStats:
    """Instrumentation metrics for Chan's Simplify-Divide-and-Conquer algorithm."""
    recursive_calls: int = 0
    zero_slab_hits: int = 0
    one_slabs_eliminated: int = 0
    leaf_cells: int = 0
    clean_2slab_cells: int = 0
    max_depth: int = 0


def _remap_1d_coord(x: float, alpha: float, beta: float, slabs: List[Tuple[float, float]]) -> float:
    """
    Apply Chan's piecewise-linear order-preserving map phi: [alpha, beta] -> [0, beta']
    that collapses all disjoint 1-slab intervals in `slabs` to points.
    """
    if x <= alpha + EPS:
        return 0.0
    if x >= beta - EPS:
        removed = sum(v - u for u, v in slabs)
        return max(0.0, (beta - alpha) - removed)
    removed = 0.0
    for u, v in slabs:
        if x <= u + EPS:
            break
        elif x >= v - EPS:
            removed += v - u
        else:
            # x lies inside [u, v]
            removed += x - u
            break
    return max(0.0, (x - alpha) - removed)


def simplify_cell_1slabs(
    cell: List[Tuple[float, float]],
    boxes: List[Box],
    stats: Optional[ChanSDCStats] = None,
) -> Tuple[float, List[Tuple[float, float]], List[Box], bool]:
    """
    Perform Chan's (2013) iterative 0-slab detection and 1-slab elimination
    on `cell` and `boxes` (assumed already clipped to `cell`).

    Returns:
        (accumulated_1slab_volume, compressed_cell, remaining_boxes, is_fully_covered)
    where every box in `remaining_boxes` has k(B, compressed_cell) >= 2 (at least a 2-slab).
    """
    d = len(cell)
    cur_cell = [(float(a), float(b)) for a, b in cell]
    cur_boxes = list(boxes)
    accumulated_vol = 0.0

    while True:
        # Check if the cell itself has collapsed to zero volume
        cell_lengths = [cur_cell[i][1] - cur_cell[i][0] for i in range(d)]
        if any(L <= EPS for L in cell_lengths):
            return accumulated_vol, cur_cell, [], True

        cell_vol = math.prod(cell_lengths)

        # Classify boxes by their active dimensions inside cur_cell
        surviving_boxes: List[Box] = []
        one_slabs_by_dim: List[List[Tuple[float, float]]] = [[] for _ in range(d)]

        for b in cur_boxes:
            # Clip box to cur_cell and check validity
            clipped = []
            valid_box = True
            active_dims = []
            for i in range(d):
                ca, cb = cur_cell[i]
                ba = max(ca, b[i][0])
                bb = min(cb, b[i][1])
                if bb - ba <= EPS:
                    valid_box = False
                    break
                clipped.append((ba, bb))
                if ba > ca + EPS or bb < cb - EPS:
                    active_dims.append(i)
            if not valid_box:
                continue

            k_active = len(active_dims)
            if k_active == 0:
                # 0-slab: box completely covers cur_cell!
                if stats is not None:
                    stats.zero_slab_hits += 1
                return accumulated_vol + cell_vol, cur_cell, [], True
            elif k_active == 1:
                dim_i = active_dims[0]
                one_slabs_by_dim[dim_i].append(clipped[dim_i])
                if stats is not None:
                    stats.one_slabs_eliminated += 1
            else:
                surviving_boxes.append(tuple(clipped))

        # Check if any 1-slabs were found in any dimension
        if not any(one_slabs_by_dim):
            return accumulated_vol, cur_cell, surviving_boxes, False

        # Compress each dimension that has 1-slabs sequentially
        for i in range(d):
            if not one_slabs_by_dim[i]:
                continue
            ca, cb = cur_cell[i]
            L_i = cb - ca
            if L_i <= EPS:
                return accumulated_vol, cur_cell, [], True

            # Clip the 1-slabs in dim i to [ca, cb] and merge into disjoint intervals
            clipped_slabs = [
                (max(ca, u), min(cb, v))
                for u, v in one_slabs_by_dim[i]
                if min(cb, v) - max(ca, u) > EPS
            ]
            merged = union_1d_disjoint(clipped_slabs)
            if not merged:
                continue

            removed_len = sum(v - u for u, v in merged)
            # Volume covered by these 1-slabs in the current remaining cell:
            other_prod = math.prod(
                cur_cell[j][1] - cur_cell[j][0] for j in range(d) if j != i
            )
            accumulated_vol += removed_len * other_prod

            new_len = max(0.0, L_i - removed_len)
            if new_len <= EPS:
                cur_cell[i] = (0.0, 0.0)
                return accumulated_vol, cur_cell, [], True

            # Remap all surviving boxes' i-th coordinate via phi_i: [ca, cb] -> [0, new_len]
            remapped_boxes: List[Box] = []
            for b in surviving_boxes:
                new_a = _remap_1d_coord(b[i][0], ca, cb, merged)
                new_b = _remap_1d_coord(b[i][1], ca, cb, merged)
                if new_b - new_a > EPS:
                    b_list = list(b)
                    b_list[i] = (new_a, new_b)
                    remapped_boxes.append(tuple(b_list))
            surviving_boxes = remapped_boxes
            cur_cell[i] = (0.0, new_len)

        cur_boxes = surviving_boxes


def _choose_chan_split_hyperplane(
    cell: List[Tuple[float, float]],
    boxes: List[Box],
    preferred_dim: int,
) -> Tuple[int, float]:
    """
    Select a splitting dimension s and splitting coordinate h in (alpha_s, beta_s)
    using Chan's (2013) weighted median of codimension-2 ridges.
    """
    d = len(cell)
    # Compute active wall counts w_j(B) in {0, 1, 2} for each box B and dimension j
    wall_counts: List[List[int]] = []
    total_walls_per_dim = [0] * d
    for b in boxes:
        w = []
        for j in range(d):
            ca, cb = cell[j]
            cnt = (1 if b[j][0] > ca + EPS else 0) + (1 if b[j][1] < cb - EPS else 0)
            w.append(cnt)
            total_walls_per_dim[j] += cnt
        wall_counts.append(w)

    # Choose preferred_dim if it has interior walls; otherwise pick the dim with most walls
    s = preferred_dim
    if total_walls_per_dim[s] == 0:
        s = max(range(d), key=lambda j: total_walls_per_dim[j])

    ca, cb = cell[s]
    # Collect candidate interior walls (coord, ridge_weight) in dimension s
    candidates: List[Tuple[float, int]] = []
    for idx, b in enumerate(boxes):
        w = wall_counts[idx]
        # Number of active walls of B in dimensions other than s
        other_walls = sum(w[j] for j in range(d) if j != s)
        weight = max(1, other_walls)
        if b[s][0] > ca + EPS:
            candidates.append((b[s][0], weight))
        if b[s][1] < cb - EPS:
            candidates.append((b[s][1], weight))

    candidates.sort(key=lambda x: x[0])
    total_weight = sum(wt for _, wt in candidates)
    half_weight = total_weight / 2.0

    acc = 0
    split_coord = candidates[len(candidates) // 2][0]
    for coord, wt in candidates:
        acc += wt
        if acc >= half_weight:
            split_coord = coord
            break

    return s, split_coord


def klee_chan_sdc(
    boxes: List[Box],
    use_2d_base_case: bool = True,
    stats: Optional[ChanSDCStats] = None,
) -> float:
    """
    Timothy Chan's (FOCS 2013) O(n^{d/2}) Simplify, Divide, and Conquer algorithm
    for Klee's Measure Problem in R^d.
    """
    valid, d = validate_boxes(boxes)
    if not valid:
        return 0.0
    if d == 1:
        return klee_1d([b[0] for b in valid])
    if d == 2 and use_2d_base_case:
        return klee_2d_bentley(valid)

    # Bounding cell enclosing all input boxes
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

        # Step 1 & 2: 0-slab short-circuiting and iterative 1-slab elimination
        vol_1slabs, comp_cell, rem_boxes, fully_covered = simplify_cell_1slabs(
            cell, cur_boxes, stats
        )
        if fully_covered or not rem_boxes:
            if stats is not None:
                stats.leaf_cells += 1
            return vol_1slabs

        # Optional: check if all remaining boxes are 2-slabs (for instrumentation)
        if stats is not None:
            all_2slabs = True
            for b in rem_boxes:
                active = sum(
                    1
                    for i in range(d)
                    if b[i][0] > comp_cell[i][0] + EPS or b[i][1] < comp_cell[i][1] - EPS
                )
                if active >= 3:
                    all_2slabs = False
                    break
            if all_2slabs:
                stats.clean_2slab_cells += 1

        # Step 3: Choose weighted median splitting hyperplane on codimension-2 ridges
        s, h = _choose_chan_split_hyperplane(comp_cell, rem_boxes, depth % d)
        ca, cb = comp_cell[s]

        # Guard against numerical degeneracy where h is at the boundary
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
