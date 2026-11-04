"""
Prefix Covering Design Compiler, Repository Parser, and Local Search Optimizer
==============================================================================
Implements:
1. Theorem 7 Compiler (compile_covering_design_to_pcd):
   Transforms any (v, k, 2) covering design with d blocks that admits a
   multi-matching (partition into d sets of size v' = v/d) into a valid
   (d, K, alpha) Prefix Covering Design for any replication factor n >= 1,
   achieving the asymptotic lower bound:
       lim_{n -> inf} K / alpha = d / (3 - 2v / (kd)).
2. Repository Parser (parse_covering_designs_repo):
   Parses the curated La Jolla covering designs for d = 3..44.
3. Pre-compiled Library of PCDs for d = 3..13.
4. PCDLocalSearchOptimizer:
   Vectorized NumPy-accelerated simulated annealing & local search optimizer
   for discovering and testing finite prefix covering designs.
"""

from __future__ import annotations
import math
import os
import random
import re
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Set, Tuple

import numpy as np

from prefix_covering_designs import (
    PrefixCoveringVerification,
    verify_prefix_covering_design,
)


# ============================================================================
# 1. Covering Design Repository Parser
# ============================================================================

@dataclass
class CoveringDesign:
    d: int
    v: int
    k: int
    blocks: List[List[int]]

    @property
    def v_prime(self) -> int:
        return self.v // self.d

    @property
    def frequency(self) -> float:
        return (self.k * self.d) / self.v

    @property
    def asymptotic_exponent(self) -> float:
        # Theorem 7 asymptotic exponent
        return self.d / (3.0 - 2.0 * self.v / (self.k * self.d))


def parse_covering_designs_repo(filepath: str) -> Dict[int, CoveringDesign]:
    """
    Parse the covering design repository file (from Gordon / Gorbachev-Künnemann repo).
    Each section starts with 'd = ...' and contains d blocks. The first v/d elements
    of each block are its matched partition subset U_i.
    """
    designs: Dict[int, CoveringDesign] = {}
    if not os.path.exists(filepath):
        return designs

    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    sections = re.split(r"\n+d\s*=\s*", content)
    for sec in sections[1:]:
        lines = [l.strip() for l in sec.strip().split("\n") if l.strip()]
        if not lines:
            continue
        header = lines[0]
        # header format: '3, v = 3, k = 2, freq = 2, lower bound = 1.5'
        parts = header.split(",")
        try:
            d_val = int(parts[0].strip())
            v_val = int(parts[1].split("=")[1].strip())
            k_val = int(parts[2].split("=")[1].strip())
        except (ValueError, IndexError):
            continue

        blocks: List[List[int]] = []
        for l in lines[1:]:
            nums = [int(x) for x in l.split()]
            if nums:
                blocks.append(nums)
        if len(blocks) == d_val:
            designs[d_val] = CoveringDesign(d=d_val, v=v_val, k=k_val, blocks=blocks)

    return designs


# ============================================================================
# 2. Theorem 7 Compiler: Covering Design -> Prefix Covering Design
# ============================================================================

def compile_covering_design_to_pcd(
    cd: CoveringDesign,
    n: int = 1,
) -> Tuple[List[Tuple[int, ...]], int, int]:
    """
    Compile a (v, k, 2) covering design with d blocks admitting multi-matching
    into a valid (d, K, alpha) Prefix Covering Design with replication factor n >= 1.

    Following Theorem 7 (Gorbachev & Künnemann, arXiv:2303.08612):
    - v' = v / d
    - m = n*k - (n - 1)*v' (unique elements A_i per sequence)
    - K = (n*k + v') * d
    - alpha = 3*n*k - (2*n - 3)*v'
    - Each sequence s_i has length m + n*v' + n*(k - v') = 2*n*k - (n - 2)*v':
        s_i = (A_i, U_i^{(1)}, ..., U_i^{(n)}, R_i^{(n)}, ..., R_i^{(1)})
      where U_i is the first v' elements of block i, and R_i is the remaining k - v'.
    """
    d = cd.d
    v = cd.v
    k = cd.k
    v_prime = v // d
    m = n * k - (n - 1) * v_prime
    K = (n * k + v_prime) * d
    alpha = 3 * n * k - (2 * n - 3) * v_prime

    # Separate each block B_i into U_i (first v_prime elements) and R_i (remaining k - v_prime elements)
    U = [cd.blocks[i][:v_prime] for i in range(d)]
    R = [cd.blocks[i][v_prime:] for i in range(d)]

    sequences: List[Tuple[int, ...]] = []
    # Primary unique elements A_i for each sequence i
    # Elements 1 .. n*v are used by the n copies of the covering design.
    # Elements n*v + 1 .. n*v + d*m are unique A_i.
    base_cd_elements = n * v

    for i in range(d):
        seq: List[int] = []
        # 1. Prepend m unique elements A_i
        a_start = base_cd_elements + i * m + 1
        seq.extend(range(a_start, a_start + m))

        # 2. Append U_i^{(1)}, ..., U_i^{(n)}
        for j in range(1, n + 1):
            offset = (j - 1) * v
            for u in U[i]:
                seq.append(offset + u)

        # 3. Append R_i^{(n)}, ..., R_i^{(1)} (reversed design order)
        for j in range(n, 0, -1):
            offset = (j - 1) * v
            for r in R[i]:
                seq.append(offset + r)

        sequences.append(tuple(seq))

    return sequences, K, alpha


# ============================================================================
# 3. Fast Vectorized Triplet Evaluator (NumPy)
# ============================================================================

class VectorizedPCDChecker:
    """
    High-performance NumPy vectorized checker for candidate (d, K, alpha) PCDs.
    Can evaluate thousands of candidate sequences per second.
    """
    def __init__(self, d: int, K: int, alpha: int):
        self.d = d
        self.K = K
        self.alpha = alpha

        # Precompute all triplet indices (A, B, C)
        triplets = [
            (a, b, c)
            for a in range(1, K - 1)
            for b in range(a + 1, K)
            for c in range(b + 1, K + 1)
        ]
        if triplets:
            arr = np.array(triplets, dtype=np.int32)
            self.triplet_A = arr[:, 0]
            self.triplet_B = arr[:, 1]
            self.triplet_C = arr[:, 2]
            self.num_triplets = len(triplets)
        else:
            self.triplet_A = np.zeros(0, dtype=np.int32)
            self.triplet_B = np.zeros(0, dtype=np.int32)
            self.triplet_C = np.zeros(0, dtype=np.int32)
            self.num_triplets = 0

    def evaluate(self, sequences: List[Tuple[int, ...]]) -> Tuple[bool, int, int, int]:
        """
        Returns (is_valid, max_singleton_sum, max_triplet_cost, num_violations).
        """
        # 1. Build position table: pos[elem, seq_idx] (1-based, 100000 if not present)
        pos = np.full((self.K + 1, self.d), 100000, dtype=np.int32)
        occurrences = [[] for _ in range(self.K + 1)]
        for i, seq in enumerate(sequences):
            for l_0, x in enumerate(seq):
                if 1 <= x <= self.K:
                    p = l_0 + 1
                    if p < pos[x, i]:
                        pos[x, i] = p
                    occurrences[x].append(p)

        # 2. Check Singleton Condition
        max_singleton_sum = 0
        for x in range(1, self.K + 1):
            occ = occurrences[x]
            if not occ:
                return False, 999999, 999999, 999999
            if len(occ) >= 2:
                s_sum = min(occ) + max(occ)
                if s_sum > max_singleton_sum:
                    max_singleton_sum = s_sum
                if s_sum > self.alpha + 1:
                    return False, max_singleton_sum, 0, 999999

        if self.num_triplets == 0:
            return True, max_singleton_sum, 0, 0

        # 3. Vectorized Triplet Check
        # min_single: shape (K+1,)
        min_single = pos.min(axis=1)

        # min_pair: shape (K+1, K+1) where min_pair[x, y] = min_i max(pos[x, i], pos[y, i])
        # Broadcasting: pos[:, None, :] shape (K+1, 1, d), pos[None, :, :] shape (1, K+1, d)
        min_pair = np.maximum(pos[:, None, :], pos[None, :, :]).min(axis=2)

        A = self.triplet_A
        B = self.triplet_B
        C = self.triplet_C

        c3 = min_single[A] + min_single[B] + min_single[C]
        c2 = np.minimum(
            np.minimum(min_pair[A, B] + min_single[C], min_pair[A, C] + min_single[B]),
            min_pair[B, C] + min_single[A],
        )

        cost = np.minimum(c2, c3)
        max_cost = int(cost.max())
        num_violations = int(np.sum(cost > self.alpha))

        is_valid = (num_violations == 0) and (max_singleton_sum <= self.alpha + 1)
        return is_valid, int(max_singleton_sum), max_cost, num_violations


# ============================================================================
# 4. PCD Local Search Optimizer
# ============================================================================

class PCDOptimizer:
    """
    Stochastic local search / simulated annealing optimizer for finding or
    optimizing finite Prefix Covering Designs.
    """
    def __init__(self, d: int, g: int, alpha: int, seed: int = 42):
        self.d = d
        self.g = g
        self.K = d * g
        self.alpha = alpha
        self.rng = random.Random(seed)
        self.checker = VectorizedPCDChecker(d, self.K, alpha)

    def optimize(
        self,
        initial_sequences: Optional[List[Tuple[int, ...]]] = None,
        max_steps: int = 5000,
        temp_init: float = 2.0,
        temp_min: float = 0.05,
    ) -> Tuple[Optional[List[Tuple[int, ...]]], int, int]:
        """
        Run simulated annealing starting from `initial_sequences` (or a greedy state).
        Returns (best_sequences, min_violations, best_tuple_cost).
        """
        # If no initial sequences, create a canonical primary prefix and random legal tail
        if initial_sequences is None:
            seqs: List[List[int]] = []
            for i in range(self.d):
                prim = list(range(i * self.g + 1, (i + 1) * self.g + 1))
                seqs.append(prim)
            # Add some tail elements respecting singleton bounds
            # For primary element x with l_min(x), tail position l must satisfy l <= alpha + 1 - l_min(x)
            candidates_for_tail = [
                x for x in range(1, self.K + 1)
                if (x - 1) % self.g + 1 < self.alpha // 2
            ]
            for i in range(self.d):
                num_tail = self.alpha - self.g - 2
                chosen = self.rng.sample(candidates_for_tail, min(num_tail, len(candidates_for_tail)))
                seqs[i].extend(chosen)
        else:
            seqs = [list(s) for s in initial_sequences]

        best_seqs = [tuple(s) for s in seqs]
        is_val, s_sum, t_cost, viols = self.checker.evaluate(best_seqs)
        best_viols = viols
        best_cost = t_cost

        if is_val:
            return best_seqs, 0, t_cost

        curr_seqs = [list(s) for s in seqs]
        curr_viols = viols

        t_start = time.perf_counter()
        decay = (temp_min / temp_init) ** (1.0 / max(1, max_steps))
        T = temp_init

        for step in range(max_steps):
            # Propose a local move in the tail (indices >= g)
            # Pick a random sequence i
            i = self.rng.randrange(self.d)
            tail_len = len(curr_seqs[i]) - self.g

            move_type = self.rng.random()
            if tail_len > 0 and move_type < 0.4:
                # 1. Swap two elements in tail of sequence i
                if tail_len >= 2:
                    p1 = self.g + self.rng.randrange(tail_len)
                    p2 = self.g + self.rng.randrange(tail_len)
                    if p1 != p2:
                        # Check singleton condition for swapped elements
                        x1, x2 = curr_seqs[i][p1], curr_seqs[i][p2]
                        # Verify swap doesn't violate singleton bound
                        lmin1 = (x1 - 1) % self.g + 1
                        lmin2 = (x2 - 1) % self.g + 1
                        if p2 + 1 + lmin1 <= self.alpha + 1 and p1 + 1 + lmin2 <= self.alpha + 1:
                            curr_seqs[i][p1], curr_seqs[i][p2] = x2, x1
                            cand = [tuple(s) for s in curr_seqs]
                            _, _, n_cost, n_viols = self.checker.evaluate(cand)
                            delta = n_viols - curr_viols
                            if delta <= 0 or (T > 0 and self.rng.random() < math.exp(-delta / T)):
                                curr_viols = n_viols
                                if curr_viols < best_viols:
                                    best_viols = curr_viols
                                    best_cost = n_cost
                                    best_seqs = cand
                                    if best_viols == 0:
                                        break
                            else:
                                curr_seqs[i][p1], curr_seqs[i][p2] = x1, x2
            elif tail_len > 0 and move_type < 0.8:
                # 2. Replace a tail element with another element not in sequence i
                p = self.g + self.rng.randrange(tail_len)
                old_x = curr_seqs[i][p]
                # Pick a candidate x that can legally appear at pos p+1
                legal_cands = [
                    x for x in range(1, self.K + 1)
                    if (x - 1) // self.g != i  # not primary in seq i
                    and x not in curr_seqs[i]  # no duplicate within seq i
                    and ((x - 1) % self.g + 1) + (p + 1) <= self.alpha + 1  # singleton bound
                ]
                if legal_cands:
                    new_x = self.rng.choice(legal_cands)
                    curr_seqs[i][p] = new_x
                    cand = [tuple(s) for s in curr_seqs]
                    _, _, n_cost, n_viols = self.checker.evaluate(cand)
                    delta = n_viols - curr_viols
                    if delta <= 0 or (T > 0 and self.rng.random() < math.exp(-delta / T)):
                        curr_viols = n_viols
                        if curr_viols < best_viols:
                            best_viols = curr_viols
                            best_cost = n_cost
                            best_seqs = cand
                            if best_viols == 0:
                                break
                    else:
                        curr_seqs[i][p] = old_x
            else:
                # 3. Swap tail elements across sequences
                j = (i + self.rng.randrange(1, self.d)) % self.d
                tail_j = len(curr_seqs[j]) - self.g
                if tail_len > 0 and tail_j > 0:
                    pi = self.g + self.rng.randrange(tail_len)
                    pj = self.g + self.rng.randrange(tail_j)
                    xi, xj = curr_seqs[i][pi], curr_seqs[j][pj]
                    lmin_i = (xi - 1) % self.g + 1
                    lmin_j = (xj - 1) % self.g + 1
                    if (
                        (xi - 1) // self.g != j and xj not in curr_seqs[i]
                        and (xj - 1) // self.g != i and xi not in curr_seqs[j]
                        and lmin_j + pi + 1 <= self.alpha + 1
                        and lmin_i + pj + 1 <= self.alpha + 1
                    ):
                        curr_seqs[i][pi], curr_seqs[j][pj] = xj, xi
                        cand = [tuple(s) for s in curr_seqs]
                        _, _, n_cost, n_viols = self.checker.evaluate(cand)
                        delta = n_viols - curr_viols
                        if delta <= 0 or (T > 0 and self.rng.random() < math.exp(-delta / T)):
                            curr_viols = n_viols
                            if curr_viols < best_viols:
                                best_viols = curr_viols
                                best_cost = n_cost
                                best_seqs = cand
                                if best_viols == 0:
                                    break
                        else:
                            curr_seqs[i][pi], curr_seqs[j][pj] = xi, xj

            T *= decay

        return best_seqs, best_viols, best_cost


# ============================================================================
# 5. High-Level Hub for Explicit & Compiled Designs
# ============================================================================

def get_best_known_design(
    d: int,
    repo_path: Optional[str] = None,
    n_replications: int = 1,
) -> Tuple[List[Tuple[int, ...]], int, int, str]:
    """
    Retrieve the best known prefix covering design for dimension d:
    - For d = 3: Closed-form cyclic design achieving K/alpha -> 1.5.
    - For d = 4: Explicit SAT-discovered (4, 40, 21) design (gamma_4 >= 40/21 > 1.90476).
    - For d = 5: Explicit SAT-discovered (5, 40, 18) design (gamma_5 >= 40/18 > 2.22222).
    - For d = 6: Cyclic difference set (6, 54, 21) design (gamma_6 >= 18/7 > 2.57142).
    - For d = 7: Fano plane design (7, 21t, 7t+1) achieving K/alpha -> 3.0.
    - For general d >= 3: Compiled from the covering design repository.
    """
    from prefix_covering_designs import (
        build_cyclic_d3_design,
        get_d4_40_21_design,
        get_d5_40_18_design,
        build_d6_difference_set_design,
        build_d7_fano_design,
    )

    if d == 3:
        seqs, K, alpha = build_cyclic_d3_design(m=10)
        return seqs, K, alpha, "Cyclic (3, 30, 21), K/alpha -> 1.50000"
    if d == 4:
        seqs, K, alpha = get_d4_40_21_design()
        return seqs, K, alpha, "Theorem 5 SAT-design (4, 40, 21), gamma_4 >= 1.90476"
    if d == 5:
        seqs, K, alpha = get_d5_40_18_design()
        return seqs, K, alpha, "Theorem 5 SAT-design (5, 40, 18), gamma_5 >= 2.22222"
    if d == 6:
        seqs, K, alpha = build_d6_difference_set_design(m=9, t=3)
        return seqs, K, alpha, "Cyclic Difference-Set (6, 54, 21), gamma_6 >= 18/7 = 2.57143"
    if d == 7:
        seqs, K, alpha = build_d7_fano_design(t=3)
        return seqs, K, alpha, "Fano Plane Difference-Set (7, 63, 22), gamma_7 -> 3.00000"

    # For d >= 8: try repository compilation
    if repo_path is None:
        default_p = os.path.join(os.path.dirname(__file__), "covering_designs_repo.txt")
        if os.path.exists(default_p):
            repo_path = default_p

    if repo_path and os.path.exists(repo_path):
        repo = parse_covering_designs_repo(repo_path)
        if d in repo:
            cd = repo[d]
            seqs, K, alpha = compile_covering_design_to_pcd(cd, n=n_replications)
            desc = (
                f"Compiled from (v={cd.v}, k={cd.k}, 2)-covering design (n={n_replications}), "
                f"asymptotic exponent >= {cd.asymptotic_exponent:.4f}"
            )
            return seqs, K, alpha, desc

    raise ValueError(f"No design found or compiled for d={d}")
