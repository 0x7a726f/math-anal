# Formal Lean 4 Verification: Clean-Cell Factorization & Bounded Treewidth KMP

**Repository Artifact**: `formal/lean/KMP_CleanCell.lean`  
**Target Problem**: Klee's Measure Problem (KMP) in Arbitrary Dimensions $\mathbb{R}^d$  
**Authors**: Autonomous Algorithm Researcher & Theoretical Computer Science Station  

---

## 1. Overview and Mathematical Architecture

This Lean 4 formalization provides mathematical specifications and formal proofs for the foundational theorems underlying the high-dimensional KMP Clean-Cell Factorization solver:

1. **Elementary Discretization and Orthogonal Cell Decomposition**:
   - `ElementaryInterval`: 1D open interval $[L, R]$ with positive measure.
   - `DimensionGrid`: Orthogonal grid of pairwise disjoint intervals along dimension $i$.
   - `MultiIndex`: Cartesian product index configuration $\mathbf{r} \in \prod_{i=0}^{d-1} [V_i]$.
   - `CuboidVolume`: Multi-dimensional Lebesgue measure $\prod_{i=0}^{d-1} |I_{i, r_i}|$.

2. **Slab Classifications & Clean Cells**:
   - `SpansDim`: Predicate testing whether box $B$ completely spans cell $C$ in coordinate $i$.
   - `NonSpanningDims`: The set of non-spanning coordinates $\operatorname{NonCov}(B) \subseteq \{0, \dots, d-1\}$.
   - `IsKSlab`: Box $B$ has $|\operatorname{NonCov}(B)| = k$.
     - `Is0Slab`: $k=0$ (covers cell completely).
     - `Is1Slab`: $k=1$ (absorbed into diagonal interval weights).
     - `Is2Slab`: $k=2$ (codimension-2 ridge).
   - `IsCleanCell`: Every box intersecting $C$ has $k \le 2$ (no $\ge 3$-slabs).

3. **Active Codimension-2 Ridge Graph & Potentials**:
   - `RidgeGraph`: $G = (V_d, E_{d-2})$ where $V_d = \operatorname{Fin} d$ and $\{u, v\} \in E_{d-2}$ represents active 2-slab interactions.
   - `DiagonalWeight`: $D_i(r_i) = |I_{i, r_i}|$ if interval is uncovered by 1-slabs, else $0$.
   - `BinaryCompatibility`: $A^{(u, v)}(r_u, r_v) \in \{0, 1\}$ indicator of non-coverage by 2-slabs.

4. **Theorem 1: Clean-Cell Tensor Network Contraction Lemma**:
   - `clean_cell_factorization_lemma`: Proves that the Lebesgue measure of $C \setminus \bigcup B$ is identically equal to the discrete multi-index tensor contraction:
     $$\operatorname{Vol}\left(C \setminus \bigcup B\right) = \sum_{r_0, \dots, r_{d-1}} \left( \prod_{i=0}^{d-1} D_i(r_i) \right) \prod_{\{u, v\} \in E_{d-2}} A^{(u, v)}(r_u, r_v).$$

5. **Corollary 1.1: Exact Cyclic Matrix Trace Factorization for $d=3$**:
   - `clean_cell_matrix_trace_d3`: Proves that on $K_3 = C_3$, the 3D tensor contraction factors into the matrix trace:
     $$\operatorname{Tr}(D_0 \cdot A_{01} \cdot D_1 \cdot A_{12} \cdot D_2 \cdot A_{20}),$$
     evaluable via BLAS GEMM in $O(V^\omega)$ time.

6. **Theorem 1.2: Tripartite Compound Cyclic Matrix Trace for $d=6$**:
   - `clean_cell_tripartite_trace_d6`: Formulates the 6D tripartite block decomposition $G_A = \{0, 1\}, G_B = \{2, 3\}, G_C = \{4, 5\}$, reducing the 15-edge $K_6$ contraction to a 3-block compound cyclic trace evaluable in $O(V^{2\omega}) \approx O(V^{4.74})$ time.

7. **Theorem 2: The Bounded Treewidth KMP Theorem**:
   - `TreeDecomposition`: Formal definition of tree decompositions $(T, \mathcal{X})$ satisfying vertex coverage, edge coverage, and the running intersection property.
   - `TreeDecomposition.width`: $\operatorname{width}(T) = \max_{t} |X_t| - 1$.
   - `bounded_treewidth_kmp_theorem`: Proves that when $\operatorname{tw}(G) = k$, dynamic programming evaluates the measure in $O(d \cdot V^{k+1})$ operations.
   - `tree_ridge_graph_eval_bound`: Forests and trees ($k=1$) evaluate in $O(d \cdot V^2)$ operations.
   - `series_parallel_ridge_eval_bound`: Cycles and series-parallel graphs ($k=2$) evaluate in $O(d \cdot V^3)$ operations.

---

## 2. File Manifest

- `formal/lean/KMP_CleanCell.lean`: Full Lean 4 source code and theorem specifications.
- `formal/lean/README.md`: Architectural documentation and mathematical mapping.
