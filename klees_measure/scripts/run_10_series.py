#!/usr/bin/env python3
"""
Master Execution Script: 10 Series of 20 Hypotheses (H-156 to H-355, 200 Total)
Enforces assertions between each series, records ledger, and logs epistemic progression.
"""

import os
import sys
import json
import time
import subprocess

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
os.chdir(ROOT_DIR)

from autonomous_campaign_200 import run_series_batch
from series_definitions import ALL_SERIES

OUTPUT_JSON = os.path.join(ROOT_DIR, "data", "campaign_200_results.json")

def main():
    start_series = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    end_series = int(sys.argv[2]) if len(sys.argv) > 2 else 10

    print(f"\n================================================================================")
    print(f" LAUNCHING 10-SERIES DISCOVERY CAMPAIGN (H-156 to H-355: 200 HYPOTHESES)")
    print(f" Target Series: {start_series} through {end_series}")
    print(f"================================================================================")

    all_results = {}
    if os.path.exists(OUTPUT_JSON):
        try:
            with open(OUTPUT_JSON, "r", encoding="utf-8") as f:
                all_results = json.load(f)
        except Exception:
            all_results = {}

    for s_idx in range(start_series, end_series + 1):
        series_data = ALL_SERIES[s_idx - 1]
        print(f"\n>>> EXECUTING SERIES {s_idx}/10: {series_data[0][0]} to {series_data[-1][0]} (20 HYPOTHESES)...")
        t0 = time.time()
        results = run_series_batch(s_idx, series_data)
        dt = time.time() - t0

        # Assertions
        verified_count = sum(1 for r in results if r["verified"])
        assert verified_count == len(results), f"CRITICAL ASSERTION FAILURE in Series {s_idx}: Volume mismatch detected!"

        pareto_count = sum(1 for r in results if r["status"] == "PARETO_NEW_BEST")
        regression_count = sum(1 for r in results if r["status"] == "REGRESSION")

        series_summary = {
            "series": s_idx,
            "range": f"{series_data[0][0]}..{series_data[-1][0]}",
            "count": len(results),
            "verified_count": verified_count,
            "pareto_count": pareto_count,
            "regression_count": regression_count,
            "elapsed_seconds": round(dt, 2),
            "hypotheses": results
        }

        all_results[f"series_{s_idx}"] = series_summary

        with open(OUTPUT_JSON, "w", encoding="utf-8") as f:
            json.dump(all_results, f, indent=2)

        # Run empirical assertion pipeline milestone
        print(f"[*] Running empirical assertion pipeline for Series {s_idx}...")
        pipeline_cmd = [
            sys.executable, "harness/empirical_assertion_pipeline.py",
            "--series", str(s_idx),
            "--hypothesis", f"Series-{s_idx}-Milestone",
            "--note", f"Verified {len(results)}/{len(results)} exact volume equality, active faces = {results[-1]['total_d2_faces']}, alpha_hat = {results[-1]['alpha_hat']}"
        ]
        res_p = subprocess.run(pipeline_cmd, capture_output=True, text=True)
        print(f"    Assertion Output: {'PASS' if res_p.returncode == 0 else 'FAIL'}")

        print(f"\n[+] Series {s_idx}/10 COMPLETED in {dt:.2f}s with 100% exact verification asserted.")

    print(f"\n================================================================================")
    print(f" 10-SERIES CAMPAIGN (H-156 to H-355) EXECUTION COMPLETE")
    print(f" Total Evaluated: {sum(len(s['hypotheses']) for s in all_results.values())} hypotheses")
    print(f" All 100% Exact Volume Verified.")
    print(f"================================================================================")

if __name__ == "__main__":
    main()
