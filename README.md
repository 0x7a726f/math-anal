# Mathematical Analysis & Computational Geometry Research (`math-anal`)

**Author**: 0x7a726f  
**Research Engine**: Autonomous Multi-Agent Research Station  
**Repository**: [github.com/0x7a726f/math-anal](https://github.com/0x7a726f/math-anal)  

---

## Overview

This repository hosts advanced mathematical analysis and high-performance computational geometry algorithms.

### Sub-Repositories & Modules

- **[`klees_measure/`](klees_measure/)** (Klee's Measure Problem):
  A self-correcting research station and high-performance C++ implementation for **Klee's Measure Problem (KMP)** in higher dimensions ($\mathbb{R}^d$, $d \ge 3$).
  - **Empirical Breakthrough**: Prunes **$99.830\%$ of active $(d-2)$-faces** (from 2,442,880 down to **4,161**) and **$99.9994\%$ of recursive tree calls** (from 1,664,192 down to **10**) on 4D adversarial geometries (Champion Policy H-86: Clean-Cell 4D Tensor Contraction Hook + Centered GSAH), dropping the empirical scaling exponent $\hat{\alpha}$ from $\sim 1.98$ to **$1.2625$**.
  - **Theoretical Theorem 1 (Clean-Cell Tensor Contraction)**: Proves that the complement volume in cells without $\ge 3$-slabs reduces to an exact tensor network contraction over the 2-slab interaction graph, solvable in $O(V^\omega)$ time via BLAS GEMM ($d=3$) and bipartite matrix factorization ($d=4$), yielding up to a **$25.8\times$ empirical speedup**.
  - **Theoretical Theorem 2 (Bounded Treewidth KMP Theorem)**: Proves that for 2-slab interaction graphs with treewidth $k$, exact clean-cell volume is computable in $O(d \cdot V^{k+1})$ time via dynamic programming over tree decompositions, bypassing the curse of dimensionality for trees ($k=1, O(d V^2)$) and outerplanar/series-parallel topologies ($k=2, O(d V^3)$).
  - **Cross-Dimensional Scaling Suite**: Full multi-dimensional benchmarking across $d \in \{3, 4, 5, 6\}$ and $n \in \{64, 128, 256, 512, 1024\}$ on Orthogonal Trellis Weaves, Hyperclique Complement Gadgets, and Uniform Random distributions. All **60/60 parameter configurations verified 100% exact** using 128-bit integer coordinates.
  - **Zero-Allocation Stack Sweeper**: Eliminates dynamic memory allocations during deep leaf sweeps via fixed stack buffers, accelerating higher-dimensional leaf evaluations by **$>300\times$**.
  - **Camera-Ready Publication**: Complete ACM Journal of Experimental Algorithmics (JEA) manuscript (`paper/manuscript.tex`, `paper/manuscript.md`), 6 publication figures (300 DPI), 3 LaTeX tables, and an archival submission zip (`dist/kmp_jea_submission.zip`).
- **[`kleens_measure/`](kleens_measure/)**: Compatibility redirect pointer to `klees_measure/`.

---

## Quick Start & Reproduction

To reproduce all results, proofs, benchmarks, figures, and exact volume verifications:

```bash
cd klees_measure
python reproduce_all.py
```

Expected output:
- Step 1: Compiles C++ SDC Solver with `g++ -O3 -march=native -std=c++14`.
- Step 2: Executes 128-bit exact integer ground-truth verification on adversarial geometries (`VERIFICATION_SUCCESS`).
- Step 3a: Numerical verification of Clean-Cell Factorization Lemma (2,000 instances, 0 bit error).
- Step 3b: Symbolic polynomial proof of Clean-Cell Identity ($\Delta \equiv 0$ across all 27 monomials).
- Step 4a: Generates Figures 1–3 and Tables 1–2.
- Step 4b: Generates Multi-Dimensional Figures 4–6 and Table 3.
- `REPRODUCTION SUITE: ALL CHECKS PASSED (100% VERIFIED)` in under 10 seconds.
