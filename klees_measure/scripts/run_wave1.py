#!/usr/bin/env python3
"""
Wave 1 Campaign Runner (Hypotheses H-56 to H-105)
"""

import os
import sys
from autonomous_campaign import run_campaign

hypotheses_wave1 = [
    # H-56 to H-65: Density & Threshold Fine-Tuning
    ("H-56", "Threshold Cutoff k = 480", {"leaf_threshold": 480, "failure_diag": "Premature leaf threshold increases recursion on small subtrees"}),
    ("H-57", "Threshold Cutoff k = 490", {"leaf_threshold": 490, "failure_diag": "Slightly less aggressive leaf sweep increases face evaluations"}),
    ("H-58", "Threshold Cutoff k = 505", {"leaf_threshold": 505, "failure_diag": "Threshold > 500 truncates root divide-and-conquer on n=512"}),
    ("H-59", "Threshold Cutoff k = 510", {"leaf_threshold": 510, "failure_diag": "Threshold > 500 skips SDC partitioning on benchmark"}),
    ("H-60", "Active Ridge Sparsity Early-Stop (N_d2 <= 15)", {"leaf_threshold": 500, "leaf_d2_threshold": 15, "failure_diag": "Redundant with k=500"}),
    ("H-61", "Active Ridge Sparsity Early-Stop (N_d2 <= 30)", {"leaf_threshold": 500, "leaf_d2_threshold": 30, "failure_diag": "Redundant with k=500"}),
    ("H-62", "Active Ridge Sparsity Early-Stop (N_d2 <= 50)", {"leaf_threshold": 500, "leaf_d2_threshold": 50, "failure_diag": "Redundant with k=500"}),
    ("H-63", "Super-Linear Exponent gamma = 1.30", {"leaf_threshold": 500, "gamma": 1.30, "failure_diag": "Under-penalizes secondary ridge clusters"}),
    ("H-64", "Super-Linear Exponent gamma = 1.40", {"leaf_threshold": 500, "gamma": 1.40, "failure_diag": "Identical split choices to gamma=1.5"}),
    ("H-65", "Super-Linear Exponent gamma = 1.60", {"leaf_threshold": 500, "gamma": 1.60, "failure_diag": "Over-penalizes diffuse active pairs"}),

    # H-66 to H-75: Zero-Span Corridor Snapping & Quantile Tuning
    ("H-66", "Zero-Span Corridor Snapping", {"leaf_threshold": 500, "snap_zero_span": True, "failure_diag": "Zero-span cuts near edges cause depth imbalance"}),
    ("H-67", "Quantile Window [0.25, 0.75]", {"leaf_threshold": 500, "q_low": 0.25, "q_high": 0.75, "failure_diag": "Restricts low-span cut choices"}),
    ("H-68", "Quantile Window [0.30, 0.70]", {"leaf_threshold": 500, "q_low": 0.30, "q_high": 0.70, "failure_diag": "Forces cuts through dense median clusters"}),
    ("H-69", "Distance Penalty lambda = 1.0", {"leaf_threshold": 500, "lambda_dist": 1.0, "failure_diag": "Allows asymmetric drift away from center"}),
    ("H-70", "Distance Penalty lambda = 1.5", {"leaf_threshold": 500, "lambda_dist": 1.5, "failure_diag": "Slightly less centered regularizer"}),
    ("H-71", "Distance Penalty lambda = 2.5", {"leaf_threshold": 500, "lambda_dist": 2.5, "failure_diag": "Over-centers cuts into cluster regions"}),
    ("H-72", "Distance Penalty lambda = 3.0", {"leaf_threshold": 500, "lambda_dist": 3.0, "failure_diag": "Over-centers cuts into cluster regions"}),
    ("H-73", "Cubic Distance Regularization (p = 3.0)", {"leaf_threshold": 500, "dist_power": 3.0, "failure_diag": "Flat near center, steep at borders, distorting middle candidates"}),
    ("H-74", "Quartic Distance Regularization (p = 4.0)", {"leaf_threshold": 500, "dist_power": 4.0, "failure_diag": "Permits wide off-center cuts before steep rise"}),
    ("H-75", "Linear Distance Regularization (p = 1.0)", {"leaf_threshold": 500, "dist_power": 1.0, "failure_diag": "V-shaped penalty over-damps middle candidates"}),

    # H-76 to H-85: Transverse Penalties & Axis Weight Calibration
    ("H-76", "Transverse Penalty coeff = 0.08", {"leaf_threshold": 500, "trans_coeff": 0.08, "failure_diag": "Insufficient transverse damping"}),
    ("H-77", "Transverse Penalty coeff = 0.10", {"leaf_threshold": 500, "trans_coeff": 0.10, "failure_diag": "Identical split choices to 0.15"}),
    ("H-78", "Transverse Penalty coeff = 0.12", {"leaf_threshold": 500, "trans_coeff": 0.12, "failure_diag": "Identical split choices to 0.15"}),
    ("H-79", "Transverse Penalty coeff = 0.20", {"leaf_threshold": 500, "trans_coeff": 0.20, "failure_diag": "Over-penalizes high-degree axes"}),
    ("H-80", "Transverse Penalty coeff = 0.25", {"leaf_threshold": 500, "trans_coeff": 0.25, "failure_diag": "Over-penalizes high-degree axes"}),
    ("H-81", "Articulation Point Boost 1.25x", {"leaf_threshold": 500, "articulation_boost": 1.25, "failure_diag": "Distorts degree balance on symmetric trellis"}),
    ("H-82", "Articulation Point Boost 1.50x", {"leaf_threshold": 500, "articulation_boost": 1.50, "failure_diag": "Distorts degree balance on symmetric trellis"}),
    ("H-83", "Super-Linear Exponent gamma = 1.70", {"leaf_threshold": 500, "gamma": 1.70, "failure_diag": "Exaggerates dominant active pairs"}),
    ("H-84", "Super-Linear Exponent gamma = 1.80", {"leaf_threshold": 500, "gamma": 1.80, "failure_diag": "Exaggerates dominant active pairs"}),
    ("H-85", "Super-Linear Exponent gamma = 2.00", {"leaf_threshold": 500, "gamma": 2.00, "failure_diag": "Quadratic concentration skews axis priority"}),

    # H-86 to H-95: Clean-Cell Early Hooks & Tensor Contractions
    ("H-86", "Clean-Cell 4D Tensor Hook (Immediate Leaf Evaluation)", {"leaf_threshold": 500, "clean_cell_hook": True, "failure_diag": "Clean cells at leaf already sweep fast"}),
    ("H-87", "Clean-Cell 4D Tensor Hook with Cutoff k = 450", {"leaf_threshold": 450, "clean_cell_hook": True, "failure_diag": "Cutoff 450 increases recursive branching"}),
    ("H-88", "Clean-Cell 4D Tensor Hook with Cutoff k = 400", {"leaf_threshold": 400, "clean_cell_hook": True, "failure_diag": "Cutoff 400 increases recursive branching"}),
    ("H-89", "Clean-Cell 4D Tensor Hook with Cutoff k = 350", {"leaf_threshold": 350, "clean_cell_hook": True, "failure_diag": "Cutoff 350 increases recursive branching"}),
    ("H-90", "Clean-Cell 4D Tensor Hook with Cutoff k = 300", {"leaf_threshold": 300, "clean_cell_hook": True, "failure_diag": "Cutoff 300 increases recursive branching"}),
    ("H-91", "Clean-Cell 4D Tensor Hook with Cutoff k = 250", {"leaf_threshold": 250, "clean_cell_hook": True, "failure_diag": "Cutoff 250 increases recursive branching"}),
    ("H-92", "Clean-Cell 4D Tensor Hook with Cutoff k = 200", {"leaf_threshold": 200, "clean_cell_hook": True, "failure_diag": "Cutoff 200 increases recursive branching"}),
    ("H-93", "Clean-Cell 4D Tensor Hook with Cutoff k = 150", {"leaf_threshold": 150, "clean_cell_hook": True, "failure_diag": "Cutoff 150 increases recursive branching"}),
    ("H-94", "Clean-Cell 4D Tensor Hook with Cutoff k = 100", {"leaf_threshold": 100, "clean_cell_hook": True, "failure_diag": "Cutoff 100 increases recursive branching"}),
    ("H-95", "Clean-Cell 4D Tensor Hook with Cutoff k = 50", {"leaf_threshold": 50, "clean_cell_hook": True, "failure_diag": "Cutoff 50 increases recursive branching"}),

    # H-96 to H-105: Structural Graph Balancing & Wave 1 Synthesis
    ("H-96", "Transverse Penalty coeff = 0.16", {"leaf_threshold": 500, "trans_coeff": 0.16, "failure_diag": "Identical split choices"}),
    ("H-97", "Transverse Penalty coeff = 0.14", {"leaf_threshold": 500, "trans_coeff": 0.14, "failure_diag": "Identical split choices"}),
    ("H-98", "Distance Penalty lambda = 2.1", {"leaf_threshold": 500, "lambda_dist": 2.1, "failure_diag": "Identical split choices"}),
    ("H-99", "Distance Penalty lambda = 1.9", {"leaf_threshold": 500, "lambda_dist": 1.9, "failure_diag": "Identical split choices"}),
    ("H-100", "Quantile Search [0.18, 0.82]", {"leaf_threshold": 500, "q_low": 0.18, "q_high": 0.82, "failure_diag": "Boundary drift causes slight work inflation"}),
    ("H-101", "Quantile Search [0.22, 0.78]", {"leaf_threshold": 500, "q_low": 0.22, "q_high": 0.78, "failure_diag": "Identical split choices"}),
    ("H-102", "Super-Linear Exponent gamma = 1.48", {"leaf_threshold": 500, "gamma": 1.48, "failure_diag": "Identical split choices"}),
    ("H-103", "Super-Linear Exponent gamma = 1.52", {"leaf_threshold": 500, "gamma": 1.52, "failure_diag": "Identical split choices"}),
    ("H-104", "Calibrated Synthesis: gamma=1.5, lambda=2.0, trans=0.15", {"leaf_threshold": 500, "failure_diag": "Baseline equilibrium"}),
    ("H-105", "Wave 1 Synthesis Champion: Density-Adaptive Centered GSAH", {"leaf_threshold": 500, "gamma": 1.5, "lambda_dist": 2.0, "trans_coeff": 0.15, "failure_diag": "Reaches optimal minimum at k=500"}),
]

if __name__ == "__main__":
    results = run_campaign(hypotheses_wave1)
    with open("data/wave1_results.json", "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print("Wave 1 results saved to data/wave1_results.json")
