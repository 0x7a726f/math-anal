#!/usr/bin/env python3
"""
Autonomous Multi-Hypothesis Discovery Campaign Driver
Executes a sequence of mathematical split policies against the exact ground-truth verifier.
"""

import os
import sys
import subprocess
import re
import json

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
os.chdir(ROOT_DIR)

POLICY_FILE = os.path.join(ROOT_DIR, "solver", "split_policy.hpp")
LEADERBOARD_FILE = os.path.join(ROOT_DIR, "research", "LEADERBOARD.md")
GRAVEYARD_FILE = os.path.join(ROOT_DIR, "research", "GRAVEYARD.md")

def get_champion():
    if not os.path.exists(LEADERBOARD_FILE):
        return 999999999, 99.0
    with open(LEADERBOARD_FILE, "r", encoding="utf-8") as f:
        for line in f:
            if "CHAMPION" in line:
                parts = [p.strip().replace("*", "") for p in line.split("|") if p.strip()]
                if len(parts) >= 5:
                    try:
                        alpha = float(parts[3])
                        faces = int(parts[4].replace(",", ""))
                        return faces, alpha
                    except ValueError:
                        pass
    return 4938, 1.36792

def generate_policy_code(hypo_id, desc, params):
    """
    Generates solver/split_policy.hpp from structured mathematical parameters.
    """
    leaf_threshold = params.get("leaf_threshold", 500)
    leaf_d2_threshold = params.get("leaf_d2_threshold", 0)
    gamma = params.get("gamma", 1.5)
    trans_coeff = params.get("trans_coeff", 0.15)
    lambda_dist = params.get("lambda_dist", 2.0)
    q_low = params.get("q_low", 0.20)
    q_high = params.get("q_high", 0.80)
    dist_power = params.get("dist_power", 2.0)
    snap_zero_span = params.get("snap_zero_span", False)
    joint_lookahead = params.get("joint_lookahead", False)
    clean_cell_hook = params.get("clean_cell_hook", False)
    articulation_boost = params.get("articulation_boost", 1.0)
    factor_spanning = params.get("factor_spanning", False)

    code = f"""#pragma once
#include "../harness/types.hpp"
#include "clean_cell_trace.hpp"
#include "tensor_clean_cell.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace kmp {{

struct SplitPolicy {{
    std::string id = "{hypo_id}: {desc}";

    int leaf_box_threshold = {leaf_threshold};
    uint64_t leaf_d2_face_threshold = {leaf_d2_threshold};
    double cut_plane_ratio = 0.50;

    double get_face_weight(int i, int j, int d) const {{
        return 1.0;
    }}

    int select_split_axis(
        const Box& cell,
        const std::vector<Box>& boxes,
        const std::vector<std::vector<uint32_t>>& d2_face_matrix,
        int last_split_dim
    ) const {{
        int d = cell.dim();
        int best_dim = -1;
        double max_efficiency = -1.0;

        for (int i = 0; i < d; ++i) {{
            double weighted_deg = 0.0;
            for (int j = 0; j < d; ++j) {{
                if (i != j && d2_face_matrix[i][j] > 0) {{
                    double n_ij = (double)d2_face_matrix[i][j];
                    weighted_deg += std::pow(n_ij, {gamma});
                }}
            }}
            if (weighted_deg <= 0.0) continue;

            int spanning_count = 0;
            int transverse_spanning = 0;
            for (const auto& b : boxes) {{
                if (b.low[i] <= cell.low[i] && b.high[i] >= cell.high[i]) {{
                    spanning_count++;
                    int other_noncov = 0;
                    for (int j = 0; j < d; ++j) {{
                        if (j != i && (b.low[j] > cell.low[j] || b.high[j] < cell.high[j])) {{
                            other_noncov++;
                        }}
                    }}
                    if (other_noncov >= 2) transverse_spanning++;
                }}
            }}

            double denom = (1.0 + (double)spanning_count);
            double trans_penalty = (1.0 + {trans_coeff} * (double)transverse_spanning);
            double art_factor = {articulation_boost};
            double efficiency = (weighted_deg * art_factor) / (denom * denom * trans_penalty);
            if (efficiency > max_efficiency) {{
                max_efficiency = efficiency;
                best_dim = i;
            }}
        }}

        if (best_dim != -1 && max_efficiency > 0.0) {{
            return best_dim;
        }}

        return (last_split_dim + 1) % d;
    }}

    int64_t select_cut_coordinate(
        int axis,
        const Box& cell,
        const std::vector<Box>& boxes
    ) const {{
        int64_t c_low = cell.low[axis];
        int64_t c_high = cell.high[axis];
        int64_t c_mid = c_low + (c_high - c_low) / 2;
        double cell_w = (double)(c_high - c_low);

        std::vector<int64_t> coords;
        coords.reserve(boxes.size() * 2);
        for (const auto& b : boxes) {{
            if (b.low[axis] > c_low && b.low[axis] < c_high) coords.push_back(b.low[axis]);
            if (b.high[axis] > c_low && b.high[axis] < c_high) coords.push_back(b.high[axis]);
        }}

        if (coords.empty()) {{
            return c_mid;
        }}

        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());

        size_t start_idx = (size_t)std::floor({q_low} * coords.size());
        size_t end_idx = (size_t)std::ceil({q_high} * coords.size());
        if (start_idx >= coords.size()) start_idx = 0;
        if (end_idx > coords.size() || end_idx <= start_idx) end_idx = coords.size();

        {"// Zero-span snap check" if snap_zero_span else ""}
        {"for (size_t i = start_idx; i < end_idx; ++i) {" if snap_zero_span else ""}
        {"    int64_t cand = coords[i];" if snap_zero_span else ""}
        {"    int sc = 0;" if snap_zero_span else ""}
        {"    for (const auto& b : boxes) { if (b.low[axis] < cand && b.high[axis] > cand) { sc++; break; } }" if snap_zero_span else ""}
        {"    if (sc == 0) return cand;" if snap_zero_span else ""}
        {"}" if snap_zero_span else ""}

        int64_t best_c = coords[coords.size() / 2];
        double min_cost = 1e18;

        for (size_t i = start_idx; i < end_idx; ++i) {{
            int64_t cand = coords[i];
            int split_count = 0;
            for (const auto& b : boxes) {{
                if (b.low[axis] < cand && b.high[axis] > cand) {{
                    split_count++;
                }}
            }}
            double dist_norm = (double)std::abs(cand - c_mid) / cell_w;
            double cost = (double)split_count + {lambda_dist} * std::pow(dist_norm, {dist_power});
            if (cost < min_cost) {{
                min_cost = cost;
                best_c = cand;
            }}
        }}

        return best_c;
    }}

    bool should_terminate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        uint64_t active_d2_faces
    ) const {{
        if (boxes.empty()) return true;
        int d = cell.dim();
        int threshold = leaf_box_threshold;
        if (d >= 6) threshold = std::min(threshold, 22);
        else if (d == 5) threshold = std::min(threshold, 24);
        else if (d == 4) threshold = std::min(threshold, {leaf_threshold});
        if ((int)boxes.size() <= threshold) return true;
        if (active_d2_faces <= leaf_d2_face_threshold) return true;
        {"if (is_clean_cell(cell, boxes)) return true;" if clean_cell_hook else ""}
        return false;
    }}

    Int128 evaluate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        int d
    ) const {{
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();

        {"if (d == 4 && is_clean_cell(cell, boxes)) { return evaluate_tensor_clean_cell_d4(cell, boxes); }" if clean_cell_hook else ""}
        if (d == 3) {{
            return evaluate_clean_cell_d3_trace(cell, boxes);
        }}
        return exact_kmp_sweep(boxes, d);
    }}
}};

inline SplitPolicy& get_split_policy() {{
    static SplitPolicy policy;
    return policy;
}}

}} // namespace kmp
"""
    return code

def run_campaign(hypotheses):
    print(f"\n=======================================================")
    print(f"STARTING AUTONOMOUS CAMPAIGN: {len(hypotheses)} HYPOTHESES")
    print(f"=======================================================")

    results = []
    for hypo_id, desc, params in hypotheses:
        print(f"\n---> Preparing {hypo_id}: {desc}")
        code = generate_policy_code(hypo_id, desc, params)
        with open(POLICY_FILE, "w", encoding="utf-8") as f:
            f.write(code)

        cmd = [sys.executable, "scripts/eval_and_record.py", f"{hypo_id}: {desc}"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        out = res.stdout + res.stderr

        is_pareto = "STATUS: PARETO_NEW_BEST" in out
        is_regression = "STATUS: REGRESSION" in out
        is_compile_err = "STATUS: COMPILE_ERROR" in out
        is_verify_fail = "STATUS: VERIFICATION_FAILED" in out

        # Parse metrics
        faces_match = re.search(r"total_d2_faces_processed:\s*([\d,]+)", out)
        alpha_match = re.search(r"alpha_hat:\s*([\d\.]+)", out)
        faces = faces_match.group(1).replace(",", "") if faces_match else "N/A"
        alpha = alpha_match.group(1) if alpha_match else "N/A"

        if is_pareto:
            status = "PARETO_NEW_BEST"
            print(f"[+] {hypo_id}: STRICT PARETO CHAMPION! Faces: {faces}, Alpha: {alpha}")
        elif is_regression:
            status = "REGRESSION"
            diag = params.get("failure_diag", "Suboptimal branching factor on adversarial instance")
            with open(GRAVEYARD_FILE, "a", encoding="utf-8") as f:
                f.write(f"| {hypo_id} | {desc} | {diag} | {alpha} | {faces} |\n")
            subprocess.run(["git", "commit", "-am", f"Log {hypo_id} in graveyard"], capture_output=True)
            print(f"[-] {hypo_id}: REGRESSION. Faces: {faces}, Alpha: {alpha}. Logged to graveyard.")
        elif is_verify_fail:
            status = "VERIFICATION_FAILED"
            print(f"[!] {hypo_id}: VERIFICATION FAILED! Rolled back.")
        elif is_compile_err:
            status = "COMPILE_ERROR"
            print(f"[!] {hypo_id}: COMPILE ERROR! Rolled back.")
        else:
            status = "UNKNOWN"
            print(f"[?] {hypo_id}: {out[-300:]}")

        results.append((hypo_id, desc, status, faces, alpha))

    print(f"\n=======================================================")
    print(f"CAMPAIGN COMPLETE: {len(results)} EVALUATED")
    print(f"=======================================================")
    return results

if __name__ == "__main__":
    pass
