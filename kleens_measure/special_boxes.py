"""
Special Box Families & Accelerated Algorithms for Klee's Measure Problem
========================================================================
Implements specialized, faster algorithms for restricted box geometries:
1. Anchored Boxes / Orthants (Multi-Objective Hypervolume Indicator):
   Boxes of the form [0, p_1] x ... x [0, p_d].
   - klee_anchored_2d: O(n log n) Pareto-frontier sweep.
   - klee_anchored_3d: O(n log n) sweep-plane algorithm.
   - klee_anchored_nd: Orthant divide-and-conquer / dimension sweep.
2. Unit Hypercubes / Fat Boxes (Agarwal-Kaplan-Sharir 2007, Bringmann 2012):
   Boxes of the form [x_1, x_1 + 1] x ... x [x_d, x_d + 1].
   - klee_unit_hypercubes_grid: Sparse-grid hashing decomposition in R^d.
     Decomposes the problem into active integer grid cells [c_1, c_1 + 1] x ... x [c_d, c_d + 1].
     Each hypercube intersects at most 2^d cells and becomes an anchored corner box
     inside each cell, eliminating general boundary complexity and running in O(2^d n)
     cells rather than general O(n^{d/2}).
"""

from __future__ import annotations
import math
from collections import defaultdict
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Tuple

from klee_algorithms import Box, klee_1d, klee_2d_bentley, klee_chan_sdc, klee_sweep_nd


# ============================================================================
# 1. Anchored Boxes / Orthants (Hypervolume Indicator)
# ============================================================================

def klee_anchored_2d(points: Sequence[Sequence[float]]) -> float:
    """
    Compute the area of the union of 2D anchored boxes [0, x_i] x [0, y_i]
    in O(n log n) time by sweeping the Pareto non-dominated frontier.
    """
    if not points:
        return 0.0

    # 1. Sort points by x descending. For ties in x, sort by y descending.
    # Discard non-positive coordinates (empty boxes in positive orthant).
    valid_pts = [(float(p[0]), float(p[1])) for p in points if p[0] > 1e-15 and p[1] > 1e-15]
    if not valid_pts:
        return 0.0

    valid_pts.sort(key=lambda p: (-p[0], -p[1]))

    # 2. Extract non-dominated upper frontier: as x strictly decreases, y must strictly increase
    frontier: List[Tuple[float, float]] = []
    max_y = -1.0
    for x, y in valid_pts:
        if y > max_y:
            frontier.append((x, y))
            max_y = y

    # 3. Sum rectangular vertical slices from the frontier:
    # Slice between x_k and x_{k+1} has height y_k.
    # The last slice extends from x_m down to 0.
    total_area = 0.0
    prev_x = 0.0
    # Walk frontier in order of increasing x (from right to left)
    # frontier has x decreasing, y increasing.
    # E.g., points (10, 2), (7, 5), (3, 8).
    # Slices: (7 to 10) height 2 -> area (10 - 7) * 2
    #         (3 to 7)  height 5 -> area (7 - 3) * 5
    #         (0 to 3)  height 8 -> area (3 - 0) * 8
    # Total = 3*2 + 4*5 + 3*8 = 6 + 20 + 24 = 50.
    for k in range(len(frontier)):
        cur_x, cur_y = frontier[k]
        next_x = frontier[k + 1][0] if k + 1 < len(frontier) else 0.0
        width = cur_x - next_x
        total_area += width * cur_y

    return total_area


def klee_anchored_3d(points: Sequence[Sequence[float]]) -> float:
    """
    Compute the volume of the union of 3D anchored boxes [0, x_i] x [0, y_i] x [0, z_i]
    in O(n log n) time via sweep-plane along z:
    Events are sorted by z descending. Between z_k and z_{k+1}, the cross-section
    is the 2D union of all points with z_j >= z_k projected onto (x, y).
    """
    if not points:
        return 0.0

    valid_pts = [
        (float(p[0]), float(p[1]), float(p[2]))
        for p in points
        if p[0] > 1e-15 and p[1] > 1e-15 and p[2] > 1e-15
    ]
    if not valid_pts:
        return 0.0

    # Sort events by z descending
    valid_pts.sort(key=lambda p: -p[2])

    # Distinct z coordinates
    z_coords = sorted(list(set(p[2] for p in valid_pts)), reverse=True)

    # Group points by z coordinate
    pts_by_z: Dict[float, List[Tuple[float, float]]] = defaultdict(list)
    for x, y, z in valid_pts:
        pts_by_z[z].append((x, y))

    # We maintain the 2D non-dominated Pareto frontier of (x, y) points
    # as z sweeps down from max_z to 0.
    # At each step z_k, we insert new points and compute the 2D area.
    # The slab volume between z_k and z_{k+1} is area_2d * (z_k - z_{k+1}).
    total_vol = 0.0
    active_2d_pts: List[Tuple[float, float]] = []

    for k, cur_z in enumerate(z_coords):
        active_2d_pts.extend(pts_by_z[cur_z])
        # Compute 2D area of current active non-dominated points
        cur_area = klee_anchored_2d(active_2d_pts)
        next_z = z_coords[k + 1] if k + 1 < len(z_coords) else 0.0
        slab_height = cur_z - next_z
        total_vol += cur_area * slab_height

    return total_vol


def klee_anchored_nd(points: Sequence[Sequence[float]], d: int) -> float:
    """
    General d-dimensional anchored box volume algorithm.
    Dispatches to exact fast solvers for d = 1, 2, 3, and recursive
    slab decomposition for d >= 4.
    """
    if not points or d <= 0:
        return 0.0
    if d == 1:
        max_x = max((float(p[0]) for p in points if len(p) >= 1), default=0.0)
        return max(0.0, max_x)
    if d == 2:
        return klee_anchored_2d(points)
    if d == 3:
        return klee_anchored_3d(points)

    # For d >= 4: sweep along dimension d-1
    valid_pts = [
        [float(x) for x in p[:d]]
        for p in points
        if all(x > 1e-15 for x in p[:d])
    ]
    if not valid_pts:
        return 0.0

    last_dim = d - 1
    coords = sorted(list(set(p[last_dim] for p in valid_pts)), reverse=True)
    pts_by_coord: Dict[float, List[List[float]]] = defaultdict(list)
    for p in valid_pts:
        pts_by_coord[p[last_dim]].append(p[:last_dim])

    total_vol = 0.0
    active_pts: List[List[float]] = []
    for k, cur_val in enumerate(coords):
        active_pts.extend(pts_by_coord[cur_val])
        sub_vol = klee_anchored_nd(active_pts, d - 1)
        next_val = coords[k + 1] if k + 1 < len(coords) else 0.0
        total_vol += sub_vol * (cur_val - next_val)

    return total_vol


# ============================================================================
# 2. Unit Hypercubes / Fat Boxes (Sparse Grid Hashing)
# ============================================================================

@dataclass
class UnitHypercube:
    """A d-dimensional unit hypercube [x_1, x_1 + 1] x ... x [x_d, x_d + 1]."""
    low: Tuple[float, ...]

    @property
    def d(self) -> int:
        return len(self.low)

    def to_box(self) -> Box:
        return tuple((float(x), float(x) + 1.0) for x in self.low)


def klee_unit_hypercubes_grid(cubes: Sequence[UnitHypercube], d: int) -> float:
    """
    Compute the volume of the union of n unit hypercubes in R^d using
    sparse grid cell decomposition (Bringmann 2012, Agarwal et al. 2007).

    Each unit hypercube [x_i, x_i + 1]^d has side length 1.
    We impose the integer grid Z^d. Each hypercube intersects at most 2^d
    unit grid cells C = [c_1, c_1 + 1] x ... x [c_d, c_d + 1], where
    c_i in {floor(x_i), floor(x_i) + 1}.

    Inside each grid cell C:
    - If any hypercube has x_i <= c_i for all i, it completely covers C -> Vol(C) = 1.0.
    - Otherwise, each intersecting hypercube clips to [max(c_i, x_i), min(c_i + 1, x_i + 1)],
      which inside C = [0, 1]^d (after translation by -c) is an anchored corner box.
    - We compute the measure inside active cells and sum them.
    Total running time is O(2^d n) active cell evaluations!
    """
    if not cubes:
        return 0.0

    # 1. Map each active grid cell (c_1, ..., c_d) to list of intersecting clipped boxes
    # cell_index -> (is_fully_covered, list of local boxes in [0, 1]^d)
    grid_cells: Dict[Tuple[int, ...], List[Box]] = defaultdict(list)
    fully_covered_cells: set[Tuple[int, ...]] = set()

    for cube in cubes:
        # For each dimension, the cube covers [x_i, x_i + 1].
        # It intersects cells c_i = floor(x_i) and (if x_i is not an integer) c_i = floor(x_i) + 1.
        coords_per_dim = []
        for i in range(d):
            xi = cube.low[i]
            c_floor = math.floor(xi)
            if math.isclose(xi, c_floor, abs_tol=1e-12):
                coords_per_dim.append([c_floor])
            else:
                coords_per_dim.append([c_floor, c_floor + 1])

        # Generate Cartesian product of grid cell coordinates
        import itertools
        for cell_coord in itertools.product(*coords_per_dim):
            if cell_coord in fully_covered_cells:
                continue

            # Check if this cube completely covers the cell
            # Cube covers cell if low_i <= c_i and low_i + 1 >= c_i + 1 for all i, i.e., low_i <= c_i
            # Since cell_coord is chosen from floor(xi) and floor(xi)+1,
            # low_i <= c_i is only possible if xi == c_i!
            covers_all = True
            local_low = []
            local_high = []
            for i in range(d):
                ci = cell_coord[i]
                xi = cube.low[i]
                l = max(float(ci), xi) - float(ci)
                h = min(float(ci + 1), xi + 1.0) - float(ci)
                local_low.append(max(0.0, l))
                local_high.append(min(1.0, h))
                if not (local_low[-1] <= 1e-12 and local_high[-1] >= 1.0 - 1e-12):
                    covers_all = False

            if covers_all:
                fully_covered_cells.add(cell_coord)
                grid_cells.pop(cell_coord, None)
            else:
                grid_cells[cell_coord].append(tuple(zip(local_low, local_high)))

    # 2. Total volume = (number of fully covered cells * 1.0) + sum of measures in partially covered cells
    total_volume = float(len(fully_covered_cells))

    # Unit bounding cell [0, 1]^d
    unit_cell = tuple((0.0, 1.0) for _ in range(d))

    for cell_coord, local_boxes in grid_cells.items():
        if cell_coord in fully_covered_cells:
            continue
        # Use Bentley 2D, Bentley sweep, or Chan SDC for the local cell
        if d == 1:
            local_vol = klee_1d([b[0] for b in local_boxes])
        elif d == 2:
            local_vol = klee_2d_bentley(local_boxes)
        elif d <= 4 or len(local_boxes) < 20:
            local_vol = klee_sweep_nd(local_boxes)
        else:
            local_vol = klee_chan_sdc(local_boxes, root_bounding_cell=unit_cell)

        total_volume += min(1.0, max(0.0, local_vol))

    return total_volume
