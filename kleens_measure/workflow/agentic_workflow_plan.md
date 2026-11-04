# Multi-Agent Execution Plan: KMP Algorithm Engineering & JEA Submission

**Project**: Simplify, Divide, and Conquer for Klee's Measure Problem (KMP)  
**Target Venue**: ACM Journal of Experimental Algorithmics (JEA) / ALENEX  
**Repository**: `E:\code\math\klees_measure_problem`  

---

## 1. Executive Summary & Workflow Objectives

Having successfully established the core research infrastructure, falsified 25 mathematical hypotheses, and achieved a **98.0% reduction in active ridges** and a **99.97% reduction in recursive calls**, this plan details the autonomous multi-agent execution workflow required to finalize the empirical and theoretical package for publication.

The autonomous agentic workflow coordinates three specialized subagents operating with strict epistemic guardrails:

```
                  +-----------------------------------+
                  |      Main Supervisor Agent        |
                  |     (Antigravity Coordinator)     |
                  +-----------------+-----------------+
                                    |
          +-------------------------+-------------------------+
          |                                                   |
+---------v-------------+                   +-----------------v---+
|  Agent 1: Scaling &   |                   |  Agent 2: Formal    |
|  Ablation Worker      |                   |  Verification Worker|
|  (`scale-agent`)      |                   |  (`proof-agent`)    |
+---------+-------------+                   +-----------------+---+
          |                                                   |
          +-------------------------+-------------------------+
                                    |
                  +-----------------v-----------------+
                  |  Agent 3: Packaging & Release     |
                  |  (`release-agent`)                |
                  +-----------------------------------+
```

---

## 2. Subagent Architecture & Operational Specifications

### Agent 1: Scaling & Ablation Worker (`scale-agent`)
- **Primary Mission**: Execute massive multidimensional scaling benchmarks across dimensions, box counts, and structural geometries.
- **Experimental Parameter Matrix**:
  - Dimensions: $d \in \{3, 4, 5, 6\}$
  - Box Counts: $n \in \{64, 128, 256, 512, 1024, 2048\}$
  - Geometric Families:
    1. *Random Uniform Boxes* (independent interval bounds)
    2. *Orthogonal Trellis Weaves* (transverse 2-slab grid)
    3. *Hyperclique Complement Gadgets* (dense 3-uniform hypergraph non-edges)
- **Measured Metrics**:
  - Wall-clock time ($\mu\text{s}$ per query)
  - Peak active $(d-2)$-faces in memory
  - Total $(d-2)$-faces processed
  - Total recursive calls and leaf count
  - 128-bit exact volume verification hash
- **Deliverables**:
  - `data/scaling_matrix.json`: Structured raw telemetry.
  - High-dimensional scaling plots updating `paper/figure1_scaling.png`.

### Agent 2: Formal Verification & Symbolic Matrix-Trace Worker (`proof-agent`)
- **Primary Mission**: Formally verify the Clean-Cell Factorization Lemma and test edge-case matrix traces against combinatorial ground truth.
- **Execution Protocol**:
  - Implement a dedicated Python / SymPy verification harness (`tests/verify_clean_cell.py`).
  - Generate 10,000 randomized Clean cells (cells with zero $\ge 3$-slabs) in dimensions $d \in \{3, 4, 5\}$.
  - Compute the complement volume using:
    1. *Combinatorial Hyperplane Sweep* (exact reference).
    2. *Inclusion-Exclusion Principle* over elementary intervals.
    3. *Matrix Trace Formula*: $\operatorname{Tr}(D_1 A_{12} D_2 A_{23} D_3 A_{31})$.
  - Verify exact arithmetic equality across all test instances.
- **Deliverables**:
  - `tests/clean_cell_test_report.json`: Verification certificates.
  - Section 4.2 mathematical proof refinement in `paper/manuscript.tex`.

### Agent 3: Packaging & Release Worker (`release-agent`)
- **Primary Mission**: Assemble the publication submission archive, reproduction scripts, and standalone open-source library.
- **Execution Protocol**:
  - Generate a standardized `CMakeLists.txt` supporting GCC, Clang, and MSVC.
  - Package standalone C++ header library (`include/kmp/kmp.hpp`).
  - Create a 1-command reproduction script (`reproduce_all.py` / `reproduce.bat`).
  - Compile the LaTeX manuscript into `paper/manuscript.pdf` (or bundle clean TeX artifacts).
  - Generate an open-source `README.md` with benchmark badges and build instructions.
- **Deliverables**:
  - Complete, reproducible submission archive in `dist/kmp_jea_submission.zip`.

---

## 3. Strict Guardrails & Safety Invariants

All agents must adhere to the following non-negotiable operational rules:
1. **100% Exact Verification Invariant**: Any candidate optimization or code modification that causes a volume mismatch of even $\pm 1$ integer unit on any test instance triggers an immediate, un-overridable rollback (`git checkout`).
2. **Read-Only Test Harness**: Files in `harness/` and `scripts/` are frozen and read-only.
3. **Deterministic Seed Control**: All randomized benchmarks must use fixed 64-bit PRNG seeds to ensure bit-for-bit reproducible results.
4. **Epistemic Memory Logging**: Every regressive or neutral experiment must be diagnosed and appended to `research/GRAVEYARD.md`.

---

## 4. Execution Phases & Timeline

| Phase | Description | Responsible Agent | Expected Duration |
|:---|:---|:---|:---|
| **Phase A** | Matrix-Trace Symbolic Proof & Cross-Validation | `proof-agent` | ~5 min |
| **Phase B** | High-Dimensional Scaling Grid ($d \le 6, n \le 2048$) | `scale-agent` | ~15 min |
| **Phase C** | Manuscript Finalization & Artifact Bundling | `release-agent` | ~5 min |

---

## 5. Next Step: User Confirmation Gate

Before launching these autonomous subagents to execute Phase A, Phase B, and Phase C, the system requires confirmation from the user to proceed.
