#!/usr/bin/env python3
"""
Packages all submission artifacts into dist/kmp_jea_submission.zip
Updated with complete higher-dimensional tensor contraction and treewidth artifacts.
"""

import os
import zipfile
import shutil

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DIST_DIR = os.path.join(ROOT_DIR, "dist")
ZIP_PATH = os.path.join(DIST_DIR, "kmp_jea_submission.zip")

FILES_TO_INCLUDE = [
    # Core Headers & Solver
    ("harness/types.hpp", "harness/types.hpp"),
    ("harness/generators.hpp", "harness/generators.hpp"),
    ("harness/verifier.hpp", "harness/verifier.hpp"),
    ("solver/kmp_solver.hpp", "solver/kmp_solver.hpp"),
    ("solver/split_policy.hpp", "solver/split_policy.hpp"),
    ("solver/champion_policy.hpp", "solver/champion_policy.hpp"),
    ("solver/baseline_policy.hpp", "solver/baseline_policy.hpp"),
    ("solver/clean_cell_trace.hpp", "solver/clean_cell_trace.hpp"),
    ("solver/tensor_clean_cell.hpp", "solver/tensor_clean_cell.hpp"),
    ("solver/main.cpp", "solver/main.cpp"),
    ("include/kmp/kmp.hpp", "include/kmp/kmp.hpp"),
    
    # Paper & Artifacts
    ("paper/manuscript.tex", "paper/manuscript.tex"),
    ("paper/manuscript.md", "paper/manuscript.md"),
    ("paper/references.bib", "paper/references.bib"),
    ("paper/generate_paper_artifacts.py", "paper/generate_paper_artifacts.py"),
    ("paper/generate_scaling_figures.py", "paper/generate_scaling_figures.py"),
    ("paper/figure1_scaling.png", "paper/figure1_scaling.png"),
    ("paper/figure2_recursive_calls.png", "paper/figure2_recursive_calls.png"),
    ("paper/figure3_speedup_fmm.png", "paper/figure3_speedup_fmm.png"),
    ("paper/figure4_cross_dim_runtime.png", "paper/figure4_cross_dim_runtime.png"),
    ("paper/figure5_face_reduction.png", "paper/figure5_face_reduction.png"),
    ("paper/figure6_empirical_speedup.png", "paper/figure6_empirical_speedup.png"),
    ("paper/table1_leaderboard.tex", "paper/table1_leaderboard.tex"),
    ("paper/table2_ablation.tex", "paper/table2_ablation.tex"),
    ("paper/table3_cross_dim_scaling.tex", "paper/table3_cross_dim_scaling.tex"),
    
    # Tests, Proofs & Certificates
    ("tests/verify_clean_cell.py", "tests/verify_clean_cell.py"),
    ("tests/clean_cell_test_report.json", "tests/clean_cell_test_report.json"),
    ("tests/symbolic_proof.py", "tests/symbolic_proof.py"),
    ("tests/verify_tensor_contraction.py", "tests/verify_tensor_contraction.py"),
    ("tests/tensor_contraction_report.json", "tests/tensor_contraction_report.json"),
    ("tests/verify_treewidth_dp.py", "tests/verify_treewidth_dp.py"),
    ("tests/treewidth_dp_test_report.json", "tests/treewidth_dp_test_report.json"),
    ("tests/test_cpp_matrix_trace.cpp", "tests/test_cpp_matrix_trace.cpp"),
    ("tests/test_tensor_clean_cell.cpp", "tests/test_tensor_clean_cell.cpp"),
    ("tests/bench_trace_speedup.cpp", "tests/bench_trace_speedup.cpp"),

    # Epistemic Research Logs & Scaling Data
    ("research/LEADERBOARD.md", "research/LEADERBOARD.md"),
    ("research/GRAVEYARD.md", "research/GRAVEYARD.md"),
    ("data/scaling_matrix.json", "data/scaling_matrix.json"),
    ("data/cross_dim_scaling.json", "data/cross_dim_scaling.json"),
    ("data/wave1_results.json", "data/wave1_results.json"),
    ("data/wave2_results.json", "data/wave2_results.json"),
    ("data/campaign_200_results.json", "data/campaign_200_results.json"),
    ("data/campaign_h156_h355_ledger.json", "data/campaign_h156_h355_ledger.json"),
    ("benchmarks/scaling_grid.cpp", "benchmarks/scaling_grid.cpp"),
    
    # Build & Reproduction
    ("CMakeLists.txt", "CMakeLists.txt"),
    ("reproduce_all.py", "reproduce_all.py"),
    ("README.md", "README.md"),
]

def main():
    os.makedirs(DIST_DIR, exist_ok=True)
    if os.path.exists(ZIP_PATH):
        os.remove(ZIP_PATH)

    print(f"Creating submission package at: {ZIP_PATH}")
    with zipfile.ZipFile(ZIP_PATH, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for rel_src, rel_dst in FILES_TO_INCLUDE:
            abs_src = os.path.join(ROOT_DIR, rel_src)
            if os.path.exists(abs_src):
                zipf.write(abs_src, rel_dst)
                print(f"  + Adding {rel_dst}")
            else:
                print(f"  ! Warning: {rel_src} not found, skipping.")

    size_kb = os.path.getsize(ZIP_PATH) / 1024.0
    print(f"\n[+] Successfully created submission bundle: {ZIP_PATH} ({size_kb:.1f} KB)")

if __name__ == "__main__":
    main()
