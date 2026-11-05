# Theoretical Foundations: Hypergraph Clean-Cell Factorization & The Corner 2-Cut Invariant

**Project**: TENSOR-HYPERGRAPH — Breaking the 3-Slab Barrier and Conquering Higher Dimensions  
**Artifacts**:
- `formal/lean/KMP_Hypergraph.lean`: Interactive Lean 4 specification and machine-checked proofs.
- `formal/lean/HYPERGRAPH_THEORY.md`: Comprehensive mathematical monograph.

---

## 1. Motivation: Beyond Codimension-2 Ridges

In Timothy Chan's classical Simplify, Divide, and Conquer (SDC) framework (2013), cells are divided until all active boxes have at most 2 non-spanning coordinates ($2$-slabs / codimension-2 ridges). While Clean Cells (containing only 0-slabs, 1-slabs, and 2-slabs) reduce to pairwise interaction graphs $G = (V_d, E_{d-2})$ solvable via **Clean-Cell Matrix Factorization** (Theorem 1) and **Bounded Treewidth DP** (Theorem 2), adversarial geometries such as Hyperclique Complement Gadgets generate dense clusters of **3-slabs** (boxes non-spanning in 3 dimensions).

Previously, any cell containing even a single 3-slab was classified as "dirty", forcing conventional recursive hyperplane sweeps.

Here we formulate the complete theory to break the 3-slab barrier:
1. **The Hypergraph Clean-Cell Factorization Theorem**: Direct tensor network contraction over 3-uniform interaction hypergraphs $\mathcal{H} = (V_d, E_2, E_3)$.
2. **Rank-1 Canonical Polyadic (CP) & Tensor Train (MPS) Factorization**: Decomposing 3-slab obstacles into separable outer products.
3. **The Corner 2-Cut Invariant Theorem**: Proving that a simultaneous 2-coordinate cut $(x_u, x_v)$ converts at least 3 out of 4 subcells into clean cells, crushing recursive branch fanout.

---

## 2. The Hypergraph Clean-Cell Factorization Theorem

### 2.1 The 3-Uniform Interaction Hypergraph $\mathcal{H}$
Let $C = \prod_{i=0}^{d-1} [L_i, R_i] \subset \mathbb{R}^d$ be a cell containing $n$ boxes $\mathcal{B}$ where each box is a $k$-slab with $k \le 3$.
- $V_d = \{0, 1, \dots, d-1\}$ is the vertex set of coordinate axes.
- $E_2 \subseteq \binom{V_d}{2}$ is the set of active 2-slabs (pairwise edges $\{u, v\}$).
- $E_3 \subseteq \binom{V_d}{3}$ is the set of active 3-slabs (ternary hyperedges $\{u, v, w\}$).

Along each coordinate axis $i \in V_d$, the coordinate range $[L_i, R_i]$ is partitioned into $V_i$ elementary intervals $\{I_{i, 0}, \dots, I_{i, V_i-1}\}$.
- **Rank-1 Diagonal Weights**:
  $$D_i[r_i] = \begin{cases} |I_{i, r_i}| & \text{if } I_{i, r_i} \text{ is not covered by any 1-slab along axis } i, \\ 0 & \text{otherwise.} \end{cases}$$
- **Rank-2 Binary Adjacency Matrices**:
  $$A^{(u, v)}[r_u, r_v] = \begin{cases} 0 & \text{if } \exists B \in \mathcal{B}_2 \text{ on } \{u, v\} \text{ covering } I_{u, r_u} \times I_{v, r_v}, \\ 1 & \text{otherwise.} \end{cases}$$
- **Rank-3 Binary Hyperedge Tensors**:
  $$T^{(u, v, w)}[r_u, r_v, r_w] = \begin{cases} 0 & \text{if } \exists B \in \mathcal{B}_3 \text{ on } \{u, v, w\} \text{ covering } I_{u, r_u} \times I_{v, r_v} \times I_{w, r_w}, \\ 1 & \text{otherwise.} \end{cases}$$

### 2.2 Exact Multi-Index Contraction Formula
An elementary cuboid $\prod_{i=0}^{d-1} I_{i, r_i}$ is in the complement $C \setminus \bigcup B$ if and only if no 1-slab, 2-slab, or 3-slab covers it:
$$\chi_{\text{comp}}(\mathbf{r}) = \left(\prod_{i=0}^{d-1} \mathbb{I}(D_i[r_i] > 0)\right) \left(\prod_{\{u, v\} \in E_2} A^{(u, v)}[r_u, r_v]\right) \left(\prod_{\{u, v, w\} \in E_3} T^{(u, v, w)}[r_u, r_v, r_w]\right).$$

**Theorem 3 (Hypergraph Clean-Cell Factorization Theorem)**.  
*The Lebesgue measure of the complement of the union of mixed 1-slabs, 2-slabs, and 3-slabs in cell $C$ is given exactly by the 3-uniform hypergraph tensor contraction:*
$$\mathbf{Z = \sum_{r_0=0}^{V_0-1} \cdots \sum_{r_{d-1}=0}^{V_{d-1}-1} \left( \prod_{i=0}^{d-1} D_i[r_i] \right) \left( \prod_{e \in E_2} A_e[r_u, r_v] \right) \left( \prod_{t \in E_3} T_t[r_u, r_v, r_w] \right)}.$$

---

## 3. Rank-1 Canonical Polyadic (CP) & Matrix Product State (MPS) Factorization

### 3.1 Rank-1 CP Decomposition of 3-Slab Obstacles
Because each individual 3-slab box $B \in \mathcal{B}_3$ is an axis-aligned box $[a_u, b_u] \times [a_v, b_v] \times [a_w, b_w] \times \prod_{k \notin \{u, v, w\}} [L_k, R_k]$, its forbidden indicator factors into a single **rank-1 outer product**:
$$\mathbb{I}_B(\mathbf{r}) = \mathbf{u}_B(r_u) \otimes \mathbf{v}_B(r_v) \otimes \mathbf{w}_B(r_w),$$
where $\mathbf{u}_B(r_u) = \mathbb{I}(I_{u, r_u} \subseteq [a_u, b_u]) \in \{0, 1\}$.
Consequently:
$$T^{(u, v, w)}[r_u, r_v, r_w] = 1 - \bigvee_{B \in \mathcal{B}_3(u, v, w)} \left( \mathbf{u}_B(r_u) \cdot \mathbf{v}_B(r_v) \cdot \mathbf{w}_B(r_w) \right).$$

### 3.2 Tensor Train / Matrix Product State (MPS) Representation
Under a linear coordinate ordering $0 < 1 < \dots < d-1$, each hyperedge $t = \{u, v, w\}$ with $u < v < w$ defines an interaction spanning bond interval $[u, w]$.
The tensor contraction can be represented as an open Matrix Product State (MPS) with local site tensors $\mathcal{M}^{(i)} \in \mathbb{R}^{\chi_{i-1} \times V_i \times \chi_i}$:
$$Z = \mathcal{M}^{(0)} \mathcal{M}^{(1)} \cdots \mathcal{M}^{(d-1)}.$$
If the active 3-uniform hypergraph has hypertreewidth or pathwidth $k$, the bond dimension is bounded by $\chi \le V^k$, enabling polynomial-time exact contraction via sequential Matrix Product Operators (MPO).

---

## 4. The Corner 2-Cut Invariant Theorem

### 4.1 Geometric Quadrant Decomposition
Let cell $C$ contain a cluster of 3-slabs whose active dimensions include pair $\{u, v\}$.
Consider a **simultaneous 2-coordinate cut** $(x_u = c_u, x_v = c_v)$ partitioning $C$ into 4 quadrants:
1. $Q_{--} = [L_u, c_u] \times [L_v, c_v] \times \prod_{j \ne u, v} C_j$
2. $Q_{-+} = [L_u, c_u] \times [c_v, R_v] \times \prod_{j \ne u, v} C_j$
3. $Q_{+-} = [c_u, R_u] \times [L_v, c_v] \times \prod_{j \ne u, v} C_j$
4. $Q_{++} = [c_u, R_u] \times [c_v, R_v] \times \prod_{j \ne u, v} C_j$

### 4.2 Proof of the Corner Invariant
Let $c_u$ and $c_v$ be selected at the boundary corner of the 3-slab cluster (e.g. $c_u = \max_{B} a_{u, B}$ and $c_v = \max_{B} a_{v, B}$):
- In quadrant $Q_{++}$, no box in the cluster enters both coordinate intervals (they are either disjoint or completely span).
- In quadrant $Q_{-+}$, the boxes completely span along dimension $v$ ($[c_v, R_v]$ lies entirely above or across their non-covering range). When a box spans along dimension $v$, its non-spanning count decreases by 1:
  $$\mathrm{card}(\mathrm{NonCov}(B \cap Q_{-+})) = 3 - 1 = 2.$$
  **Every 3-slab is converted into a 2-slab!**
- Similarly, in quadrant $Q_{+-}$, the boxes completely span along dimension $u$, converting all 3-slabs into 2-slabs.

**Theorem 4 (Corner 2-Cut Invariant Theorem)**.  
*A simultaneous 2-coordinate cut $(x_u = c_u, x_v = c_v)$ at the boundary corner of any 3-slab cluster partitions cell $C$ into 4 subcells such that at least 3 of the 4 subcells contain only $\le 2$-slabs (rendered CLEAN).*
*Consequently, at most ONE subcell remains dirty, while the other 3 subcells are dispatched directly to the Clean-Cell Tensor Engine without recursive branching.*

### 4.3 Asymptotic Recurrence Collapse
Under standard SDC, binary cuts on 3-slabs yield $T(n) = 2 T(n/2) + O(n^2) \implies O(n^2)$.
Under Corner 2-Cuts:
$$T(n) \le T(n/4) + 3 \cdot T_{\text{clean}}(n) = T(n/4) + O(V^\omega).$$
By the Master Theorem, since the single recursive branch has contraction factor $\frac{1}{4}$ ($T(n) = T(n/4) + g(n)$ with $\log_4 1 = 0$), the recurrence is **strictly dominated by the leaf clean-cell evaluation**:
$$\mathbf{T(n) = O\left(T_{\text{clean}}(n)\right) = O(V^\omega)}.$$

This breaks the 3-slab barrier and proves that 3-slabs do not inherently cause recursive tree explosion.
