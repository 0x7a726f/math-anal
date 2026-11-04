#pragma once

/**
 * ============================================================================
 * KMP: High-Performance Exact Lebesgue Measure Solver for Axis-Aligned Boxes
 * Based on Engineered Simplify, Divide, and Conquer (Chan 2013)
 * with Active Codimension-2 Ridge Minimization & Coarse Leaf Crossover.
 * ============================================================================
 */

#include "../../harness/types.hpp"
#include "../../harness/verifier.hpp"
#include "../../solver/split_policy.hpp"
#include "../../solver/clean_cell_trace.hpp"
#include "../../solver/tensor_clean_cell.hpp"
#include "../../solver/kmp_solver.hpp"

namespace kmp {

/**
 * Computes the exact Lebesgue volume of the union of n axis-aligned boxes in R^d.
 * Uses 128-bit exact integer accumulation and the engineered Chan SDC policy.
 */
inline Int128 compute_measure(const std::vector<Box>& boxes) {
    StructuralCounters counters;
    SplitPolicy& policy = get_split_policy();
    return KMPSolver::solve(boxes, counters, policy);
}

/**
 * Computes the exact measure while returning structural execution telemetry.
 */
inline Int128 compute_measure_instrumented(
    const std::vector<Box>& boxes,
    StructuralCounters& counters
) {
    SplitPolicy& policy = get_split_policy();
    return KMPSolver::solve(boxes, counters, policy);
}

/**
 * Ground-truth exact verification using recursive hyperplane sweep.
 */
inline Int128 verify_measure_exact(const std::vector<Box>& boxes) {
    return exact_verify_volume(boxes);
}

} // namespace kmp
