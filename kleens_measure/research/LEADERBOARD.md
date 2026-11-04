# KMP Research Station: Pareto Leaderboard

The objective is to minimize structural recursion work (`total_d2_faces_processed` and empirical scaling exponent `alpha_hat`) on $d=4$ adversarial instances across $n \in \{64, 128, 256, 512\}$ while preserving 100% exact volume verification.

| Rank | Hypothesis ID | Mechanism / Policy Description | `alpha_hat` | `total_d2_faces` | `recursive_calls` | `trellis_leaves` | Verification |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **CHAMPION** | **H-51** | **H-51: Coarse Leaf Threshold k = 500** | **1.36792** | **4,938** | **12** | **10** | **100% Exact** |
| Contender | H-50 | H-50: Coarse Leaf Threshold k = 448 | 1.49415 | 6,155 | 14 | 11 | 100% Exact |
| Contender | H-49 | H-49: Coarse Leaf Threshold k = 384 | 1.54589 | 6,766 | 16 | 12 | 100% Exact |
| Contender | H-48 | H-48: Coarse Leaf Threshold k = 320 | 1.67182 | 8,597 | 22 | 15 | 100% Exact |
| Contender | H-47 | H-47: Coarse Leaf Threshold k = 256 | 1.76180 | 10,273 | 28 | 18 | 100% Exact |
| Contender | H-46 | H-46: Coarse Leaf Threshold k = 224 | 1.98052 | 12,996 | 40 | 24 | 100% Exact |
| Contender | H-45 | H-45: Coarse Leaf Threshold k = 192 | 2.08900 | 15,743 | 56 | 32 | 100% Exact |
| Contender | H-44 | H-44: Coarse Leaf Threshold k = 160 | 2.19459 | 18,727 | 74 | 40 | 100% Exact |
| Contender | H-43 | H-43: Coarse Leaf Threshold k = 144 | 2.29161 | 21,968 | 100 | 52 | 100% Exact |
| Contender | H-36 | H-36: Strong Transverse Ridge Duplication Penalty (coeff = 0.15) | 2.34465 | 23,990 | 118 | 59 | 100% Exact |
| Contender | H-35 | H-35: Transverse Ridge Duplication Penalty in Axis Selection | 2.34516 | 24,007 | 118 | 59 | 100% Exact |
| Contender | H-34 | H-34: Centered GSAH with Coarse Leaf Cutoff (cutoff 128) | 2.34394 | 24,088 | 120 | 59 | 100% Exact |
| Contender | H-33 | H-33: Centered GSAH with Coarse Leaf Cutoff (cutoff 112) | 2.27283 | 29,234 | 170 | 82 | 100% Exact |
| Contender | H-32 | H-32: Centered GSAH with Coarse Leaf Cutoff (cutoff 96) | 2.35441 | 34,404 | 234 | 112 | 100% Exact |
| Contender | H-31 | H-31: Centered GSAH with Coarse Leaf Cutoff (cutoff 80) | 2.40504 | 39,970 | 320 | 146 | 100% Exact |
| Contender | H-30 | H-30: Centered GSAH with Coarse Leaf Cutoff (cutoff 72) | 2.45749 | 44,323 | 396 | 181 | 100% Exact |
| Contender | H-29 | H-29: Centered GSAH with Coarse Leaf Cutoff (cutoff 64) | 2.48081 | 48,429 | 486 | 216 | 100% Exact |
| Contender | H-28 | H-28: Centered GSAH with Coarse Leaf Cutoff (cutoff 56) | 2.02356 | 54,629 | 632 | 277 | 100% Exact |
| Contender | H-26 | H-26: Centered GSAH with Quadratic Distance Regularization (lambda = 2.0) | 1.94991 | 62,792 | 850 | 374 | 100% Exact |
| Contender | H-25 | H-25: GSAH Min-Cross-Cut Duplication (quantile 0.25-0.75) | 2.01782 | 63,203 | 882 | 388 | 100% Exact |
| Contender | H-21 | H-21: Super-linear ridge concentration (gamma = 1.5, cutoff 48) | 2.12855 | 68,403 | 968 | 447 | 100% Exact |
| Contender | H-18 | H-18: Leaf base-case cutoff boxes <= 64 | 2.50558 | 49,361 | 514 | 237 | 100% Exact |
| Contender | H-17 | H-17: Leaf base-case cutoff boxes <= 48 | 2.12880 | 68,584 | 970 | 450 | 100% Exact |
| Contender | H-16 | H-16: Leaf base-case cutoff boxes <= 32 | 2.14999 | 116,980 | 2,624 | 1,238 | 100% Exact |
| Contender | H-15 | H-15: Leaf base-case cutoff boxes <= 24 | 2.15986 | 173,172 | 5,338 | 2,558 | 100% Exact |
| Contender | H-14 | H-14: Leaf base-case cutoff boxes <= 16 | 2.27319 | 313,377 | 16,014 | 7,407 | 100% Exact |
| Contender | H-13 | H-13: Leaf base-case cutoff boxes <= 12 | 2.32835 | 476,481 | 36,386 | 15,495 | 100% Exact |
| Contender | H-12 | H-12: Leaf base-case cutoff boxes <= 8 | 2.36813 | 778,324 | 108,296 | 39,065 | 100% Exact |
| Contender | H-11 | H-11: Leaf base-case cutoff boxes <= 6 | 2.35389 | 1,006,316 | 214,296 | 69,489 | 100% Exact |
| Contender | H-10 | H-10: Leaf base-case cutoff boxes <= 4 | 2.27649 | 1,253,266 | 435,070 | 132,876 | 100% Exact |
| Contender | H-09 | H-09: Leaf base-case cutoff boxes <= 3 | 2.21063 | 1,359,728 | 595,320 | 182,551 | 100% Exact |
| Contender | H-08 | H-08: Leaf base-case cutoff boxes <= 2 | 2.13642 | 1,430,876 | 755,548 | 239,218 | 100% Exact |
| Contender | H-05 | H-05: Quadratic spanning box penalty | 2.08393 | 1,465,676 | 886,350 | 298,543 | 100% Exact |
| Contender | H-04 | H-04: Ridge degree penalized by spanning box duplication | 2.08109 | 1,465,252 | 890,212 | 298,756 | 100% Exact |
| Contender | H-02 | H-02: Ridge degree with alternation penalty | 2.08426 | 1,500,425 | 953,340 | 334,823 | 100% Exact |
| Contender | H-01 | H-01: Max active ridge degree axis selection | 2.10316 | 1,453,220 | 916,880 | 330,089 | 100% Exact |
| Contender | H-00 | Baseline cyclic Chan simplify-and-divide | 1.97874 | 2,442,880 | 1,664,192 | 462,410 | 100% Exact |

---

### Benchmark Definitions
- **Dimension**: $d = 4$
- **Scaling Range**: $n \in \{64, 128, 256, 512\}$
- **Adversarial Families**:
  1. **Orthogonal Trellis Weave** ($M = 10^6$, dense cross-coordinate ridges)
  2. **Hyperclique Complement Gadget** ($V = 8$ discrete vertex slots, clustered codimension-2 ridges)
- **Primary Metric**: `total_d2_faces_processed` (cumulative across all benchmark sizes)
- **Secondary Metric**: `alpha_hat` (OLS regression slope of $\log(N_{d-2})$ vs. $\log(n)$)
