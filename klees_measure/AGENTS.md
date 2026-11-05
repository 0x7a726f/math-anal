# KMP Autonomous Research Station: Agent Operations & Guardrails

Welcome to the Klee's Measure Problem (KMP) Autonomous Research Station.

## Epistemic Memory
- [Current Pareto Leaderboard](research/LEADERBOARD.md)
- [Graveyard of Falsified Hypotheses](research/GRAVEYARD.md)

## Hard Invariants & Scientific Guardrails
1. **IMMUTABLE INFRASTRUCTURE**: NEVER modify any files in `harness/` or `scripts/`. All algorithmic discovery MUST be expressed strictly inside `solver/split_policy.hpp`.
2. **ZERO VOLUME TOLERANCE**: 100% exact volume verification is mandatory. Any hypothesis that alters volume by even 1 unit is an automatic regression and is rolled back.
3. **RESEARCH MEMORY DISCIPLINE**: NEVER propose or test a hypothesis without checking `research/GRAVEYARD.md` first.
4. **ORTHOGONAL REFINEMENT**: ALWAYS change exactly ONE mathematical mechanism per experiment (e.g., axis selection potential, face weight matrix, cut quantile, or leaf threshold).
5. **ATOMIC BENCHMARKING**: ALWAYS evaluate candidate policies using `./scripts/eval_and_record.sh "<Hypothesis_ID: Description>"` (or `python scripts/eval_and_record.py`).
