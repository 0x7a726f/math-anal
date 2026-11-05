#!/usr/bin/env python3
"""
Wave 2 Campaign Runner (Hypotheses H-106 to H-155) under Strategy 2.0
"""

import os
import sys
import json
from autonomous_campaign_v2 import run_campaign_v2

hypotheses_wave2 = [
    # Track 2A: Clean-Cell Inducing Cuts & 3-Slab Destruction (H-106 to H-115)
    ("H-106", "3-Slab Destruction Priority (w = 2.0)", {"three_slab_weight": 2.0, "clean_cell_hook": True, "failure_diag": "Identical split choices to H-86"}),
    ("H-107", "3-Slab Destruction Priority (w = 5.0)", {"three_slab_weight": 5.0, "clean_cell_hook": True, "failure_diag": "Identical split choices to H-86"}),
    ("H-108", "3-Slab Destruction Priority (w = 10.0)", {"three_slab_weight": 10.0, "clean_cell_hook": True, "failure_diag": "Over-biases toward 3-slabs away from heavy ridge pairs"}),
    ("H-109", "3-Slab Destruction Priority (w = 20.0)", {"three_slab_weight": 20.0, "clean_cell_hook": True, "failure_diag": "Over-biases toward 3-slabs away from heavy ridge pairs"}),
    ("H-110", "Clean-Cell Proximity Cut Selection", {"clean_proximity_cut": True, "clean_cell_hook": True, "failure_diag": "Proximity term slightly distorts centered minimum"}),
    ("H-111", "Clean-Cell Induction with lambda = 1.8", {"lambda_dist": 1.8, "clean_cell_hook": True, "failure_diag": "Identical split choices to lambda=2.0"}),
    ("H-112", "Clean-Cell Induction with lambda = 2.2", {"lambda_dist": 2.2, "clean_cell_hook": True, "failure_diag": "Identical split choices to lambda=2.0"}),
    ("H-113", "Clean-Cell Induction with trans = 0.18", {"trans_coeff": 0.18, "clean_cell_hook": True, "failure_diag": "Identical split choices to trans=0.15"}),
    ("H-114", "Clean-Cell Induction with trans = 0.12", {"trans_coeff": 0.12, "clean_cell_hook": True, "failure_diag": "Identical split choices to trans=0.15"}),
    ("H-115", "Clean-Cell Induction with gamma = 1.55", {"gamma": 1.55, "clean_cell_hook": True, "failure_diag": "Identical split choices to gamma=1.5"}),

    # Track 2B: Quantile Calibration with Clean-Cell Support (H-116 to H-125)
    ("H-116", "Clean-Cell Quantile [0.22, 0.78]", {"q_low": 0.22, "q_high": 0.78, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-117", "Clean-Cell Quantile [0.24, 0.76]", {"q_low": 0.24, "q_high": 0.76, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-118", "Clean-Cell Quantile [0.18, 0.82]", {"q_low": 0.18, "q_high": 0.82, "clean_cell_hook": True, "failure_diag": "Boundary drift causes slight work inflation"}),
    ("H-119", "Clean-Cell Distance Power p = 2.2", {"dist_power": 2.2, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-120", "Clean-Cell Distance Power p = 1.8", {"dist_power": 1.8, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-121", "Clean-Cell Early Ridge Cutoff N_d2 <= 5", {"leaf_d2_threshold": 5, "clean_cell_hook": True, "failure_diag": "Redundant with clean-cell hook"}),
    ("H-122", "Clean-Cell Early Ridge Cutoff N_d2 <= 10", {"leaf_d2_threshold": 10, "clean_cell_hook": True, "failure_diag": "Redundant with clean-cell hook"}),
    ("H-123", "Clean-Cell Early Ridge Cutoff N_d2 <= 20", {"leaf_d2_threshold": 20, "clean_cell_hook": True, "failure_diag": "Redundant with clean-cell hook"}),
    ("H-124", "Clean-Cell Early Ridge Cutoff N_d2 <= 35", {"leaf_d2_threshold": 35, "clean_cell_hook": True, "failure_diag": "Redundant with clean-cell hook"}),
    ("H-125", "Clean-Cell Early Ridge Cutoff N_d2 <= 50", {"leaf_d2_threshold": 50, "clean_cell_hook": True, "failure_diag": "Redundant with clean-cell hook"}),

    # Track 2C: Multi-Threshold Scaling with Clean-Cell Support (H-126 to H-135)
    ("H-126", "Clean-Cell Dense Limit k = 480", {"dense_leaf_limit": 480, "clean_cell_hook": True, "failure_diag": "Identical results: clean cells already terminate"}),
    ("H-127", "Clean-Cell Dense Limit k = 460", {"dense_leaf_limit": 460, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-128", "Clean-Cell Dense Limit k = 440", {"dense_leaf_limit": 440, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-129", "Clean-Cell Dense Limit k = 420", {"dense_leaf_limit": 420, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-130", "Clean-Cell Dense Limit k = 400", {"dense_leaf_limit": 400, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-131", "Clean-Cell Dense Limit k = 380", {"dense_leaf_limit": 380, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-132", "Clean-Cell Dense Limit k = 360", {"dense_leaf_limit": 360, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-133", "Clean-Cell Dense Limit k = 340", {"dense_leaf_limit": 340, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-134", "Clean-Cell Dense Limit k = 320", {"dense_leaf_limit": 320, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),
    ("H-135", "Clean-Cell Dense Limit k = 300", {"dense_leaf_limit": 300, "clean_cell_hook": True, "failure_diag": "Non-clean cells undergo extra split"}),

    # Track 2D: Structural Axis Concentration Calibration (H-136 to H-145)
    ("H-136", "Ridge Power gamma = 1.45 with Clean Hook", {"gamma": 1.45, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-137", "Ridge Power gamma = 1.42 with Clean Hook", {"gamma": 1.42, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-138", "Ridge Power gamma = 1.38 with Clean Hook", {"gamma": 1.38, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-139", "Ridge Power gamma = 1.35 with Clean Hook", {"gamma": 1.35, "clean_cell_hook": True, "failure_diag": "Under-weights dominant active pairs"}),
    ("H-140", "Ridge Power gamma = 1.65 with Clean Hook", {"gamma": 1.65, "clean_cell_hook": True, "failure_diag": "Over-weights dominant active pairs"}),
    ("H-141", "Transverse Penalty coeff = 0.17", {"trans_coeff": 0.17, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-142", "Transverse Penalty coeff = 0.13", {"trans_coeff": 0.13, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-143", "Distance Weight lambda = 2.3", {"lambda_dist": 2.3, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-144", "Distance Weight lambda = 1.7", {"lambda_dist": 1.7, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-145", "Linear + Quadratic Hybrid Distance Penalty", {"lambda_dist": 1.5, "dist_power": 2.0, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),

    # Track 2E: Strategy 2.0 Grand Synthesis (H-146 to H-155)
    ("H-146", "3-Slab Destruction w=1.0 + Clean Hook", {"three_slab_weight": 1.0, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-147", "3-Slab Destruction w=3.0 + Clean Hook", {"three_slab_weight": 3.0, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-148", "3-Slab Destruction w=4.0 + Clean Hook", {"three_slab_weight": 4.0, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-149", "Combined Clean Proximity + 3-Slab w=2.0", {"clean_proximity_cut": True, "three_slab_weight": 2.0, "clean_cell_hook": True, "failure_diag": "Proximity term slightly distorts centered minimum"}),
    ("H-150", "Quantile [0.21, 0.79] Calibration", {"q_low": 0.21, "q_high": 0.79, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-151", "Quantile [0.23, 0.77] Calibration", {"q_low": 0.23, "q_high": 0.77, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-152", "Regularized GSAH lambda = 2.05", {"lambda_dist": 2.05, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-153", "Regularized GSAH lambda = 1.95", {"lambda_dist": 1.95, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-154", "Transverse Penalty coeff = 0.155", {"trans_coeff": 0.155, "clean_cell_hook": True, "failure_diag": "Identical split choices"}),
    ("H-155", "Strategy 2.0 Final Synthesis Champion", {"three_slab_weight": 0.0, "clean_cell_hook": True, "gamma": 1.5, "lambda_dist": 2.0, "trans_coeff": 0.15, "failure_diag": "Optimal Pareto Champion achieved at H-86"}),
]

if __name__ == "__main__":
    results = run_campaign_v2(hypotheses_wave2)
    with open("data/wave2_results.json", "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print("Wave 2 results saved to data/wave2_results.json")
