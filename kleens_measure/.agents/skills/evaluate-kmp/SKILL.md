---
name: evaluate-kmp
description: Evaluates and benchmarks a candidate KMP split policy against exact verifiers and adversarial scaling suites with atomic git rollback.
---

# Evaluate KMP Policy

Use this skill whenever evaluating a hypothesis for Klee's Measure Problem (KMP).

## Execution Protocol
Run the atomic evaluation script:
```bash
python scripts/eval_and_record.py "<Hypothesis_ID: Summary>"
```
or via bash:
```bash
./scripts/eval_and_record.sh "<Hypothesis_ID: Summary>"
```

## Possible Outputs
- `STATUS: COMPILE_ERROR`: Code in `solver/split_policy.hpp` failed compilation. Auto-reverted.
- `STATUS: VERIFICATION_FAILED`: Mismatch against ground-truth volume. Auto-reverted.
- `STATUS: REGRESSION`: Structural metrics did not improve over champion. Auto-reverted. Record post-mortem into `research/GRAVEYARD.md`.
- `STATUS: PARETO_NEW_BEST`: Strictly improved `total_d2_faces_processed` or `alpha_hat`. Auto-committed to git and added to `research/LEADERBOARD.md`.
