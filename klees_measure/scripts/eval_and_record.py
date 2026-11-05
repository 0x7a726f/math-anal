#!/usr/bin/env python3
"""
Atomic Evaluation & Auto-Rollback Script for KMP Research Station
=================================================================
Usage:
    python scripts/eval_and_record.py "<Hypothesis_ID: Summary>"
"""

import sys
import os
import subprocess
import json
import re

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
os.chdir(ROOT_DIR)

SOLVER_CPP = os.path.join(ROOT_DIR, "solver", "main.cpp")
SOLVER_EXE = os.path.join(ROOT_DIR, "solver", "kmp_solver.exe")
POLICY_FILE = os.path.join(ROOT_DIR, "solver", "split_policy.hpp")
LEADERBOARD_FILE = os.path.join(ROOT_DIR, "research", "LEADERBOARD.md")
GRAVEYARD_FILE = os.path.join(ROOT_DIR, "research", "GRAVEYARD.md")

def rollback_policy():
    subprocess.run(["git", "checkout", "--", "solver/split_policy.hpp"], cwd=ROOT_DIR, capture_output=True)

def get_current_champion_metrics():
    """Extract best total_d2_faces and alpha_hat from LEADERBOARD.md"""
    if not os.path.exists(LEADERBOARD_FILE):
        return 999999999999, 99.0
    with open(LEADERBOARD_FILE, "r", encoding="utf-8") as f:
        lines = f.readlines()
    for line in lines:
        if "CHAMPION" in line:
            parts = [p.strip().replace("*", "") for p in line.split("|") if p.strip()]
            if len(parts) >= 5:
                try:
                    alpha = float(parts[3])
                    faces = int(parts[4].replace(",", ""))
                    return faces, alpha
                except ValueError:
                    pass
    return 68403, 2.12855

def main():
    if len(sys.argv) < 2:
        print("Usage: eval_and_record.py '<Hypothesis_ID: Summary>'")
        sys.exit(1)

    hypothesis_desc = sys.argv[1].strip()
    hypo_id = hypothesis_desc.split(":")[0].strip()

    print(f"\n=======================================================")
    print(f"EVALUATING: {hypothesis_desc}")
    print(f"=======================================================")

    # Step 1: Compile solver
    compile_cmd = ["g++", "-std=c++14", "-O3", "-march=native", "solver/main.cpp", "-o", "solver/kmp_solver.exe"]
    res_comp = subprocess.run(compile_cmd, cwd=ROOT_DIR, capture_output=True, text=True)
    if res_comp.returncode != 0:
        print(f"Compilation error:\n{res_comp.stderr}")
        rollback_policy()
        print("STATUS: COMPILE_ERROR")
        sys.exit(1)

    # Step 2: Verification test
    test_cmd = [SOLVER_EXE, "--mode", "test"]
    res_test = subprocess.run(test_cmd, cwd=ROOT_DIR, capture_output=True, text=True)
    if res_test.returncode != 0 or "VERIFICATION_FAILED" in res_test.stdout:
        print(f"Verification failed:\n{res_test.stdout}")
        rollback_policy()
        print("STATUS: VERIFICATION_FAILED")
        sys.exit(2)

    # Step 3: Run adversarial benchmark suite
    bench_cmd = [SOLVER_EXE, "--mode", "bench"]
    res_bench = subprocess.run(bench_cmd, cwd=ROOT_DIR, capture_output=True, text=True)
    if res_bench.returncode != 0:
        print(f"Benchmark run failed:\n{res_bench.stderr}")
        rollback_policy()
        print("STATUS: BENCHMARK_ERROR")
        sys.exit(3)

    try:
        data = json.loads(res_bench.stdout)
    except json.JSONDecodeError:
        print(f"Failed to parse benchmark JSON output:\n{res_bench.stdout}")
        rollback_policy()
        print("STATUS: OUTPUT_PARSE_ERROR")
        sys.exit(4)

    total_faces = data["total_d2_faces_processed"]
    alpha_hat = data["alpha_hat"]
    calls = data["recursive_calls"]
    leaves = data["trellis_leaves"]
    trellis_d2 = data.get("trellis_d2_512", 0)
    clique_d2 = data.get("clique_d2_512", 0)

    best_faces, best_alpha = get_current_champion_metrics()

    print(f"Candidate Metrics:")
    print(f"  total_d2_faces_processed: {total_faces:,} (Current Best: {best_faces:,})")
    print(f"  alpha_hat:                {alpha_hat:.5f} (Current Best: {best_alpha:.5f})")
    print(f"  recursive_calls:          {calls:,}")
    print(f"  trellis_leaves:           {leaves:,}")
    print(f"  trellis_d2 (n=512):       {trellis_d2:,}")
    print(f"  clique_d2  (n=512):       {clique_d2:,}")

    # Check Pareto improvement condition:
    # Strictly less d2_faces OR (strictly lower alpha_hat with no significant faces explosion)
    is_better = (total_faces < best_faces) or (alpha_hat < best_alpha - 0.005 and total_faces <= int(best_faces * 1.05))

    if is_better:
        print("\n>>> STRICT PARETO IMPROVEMENT DETECTED! <<<")
        # Update LEADERBOARD.md
        with open(LEADERBOARD_FILE, "r", encoding="utf-8") as f:
            lines = f.readlines()

        # Demote previous champion
        new_lines = []
        for line in lines:
            if "| **CHAMPION** |" in line:
                demoted = line.replace("| **CHAMPION** |", "| Contender |").replace("**", "")
                new_row = (
                    f"| **CHAMPION** | **{hypo_id}** | **{hypothesis_desc}** | "
                    f"**{alpha_hat:.5f}** | **{total_faces:,}** | **{calls:,}** | **{leaves:,}** | **100% Exact** |\n"
                )
                new_lines.append(new_row)
                new_lines.append(demoted)
            else:
                new_lines.append(line)

        with open(LEADERBOARD_FILE, "w", encoding="utf-8") as f:
            f.writelines(new_lines)

        # Commit via Git
        subprocess.run(["git", "add", "solver/split_policy.hpp", "research/LEADERBOARD.md"], cwd=ROOT_DIR)
        commit_res = subprocess.run(["git", "commit", "-m", f"PARETO: {hypothesis_desc}"], cwd=ROOT_DIR, capture_output=True, text=True)
        print(commit_res.stdout)
        print("STATUS: PARETO_NEW_BEST")
        sys.exit(0)
    else:
        print("\n>>> REGRESSION OR NO PARETO GAIN <<<")
        rollback_policy()
        print("STATUS: REGRESSION")
        sys.exit(5)

if __name__ == "__main__":
    main()
