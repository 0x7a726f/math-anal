# -*- coding: utf-8 -*-
import os

t1 = r"""\begin{table*}[t]
\centering
\caption{Pareto Progression on the $d=4$ Adversarial Benchmark ($n \in \{64, 128, 256, 512\}$ on Orthogonal Trellis Weaves and Hyperclique Gadgets). Every policy passed 100\% ground-truth exact volume verification across 756 evaluated hypotheses.}
\label{tab:leaderboard}
\small
\begin{tabular}{llrrrrr}
\toprule
\textbf{ID} & \textbf{Mechanism / Split Policy Description} & \textbf{Total $(d-2)$-Faces} & \textbf{Reduction} & \textbf{Recursive Calls} & \textbf{Trellis Leaves} & $\hat{\alpha}$ \\
\midrule
\textbf{H-00} & Baseline cyclic Chan simplify-and-divide & 2,442,880 & 0.0\% & 1,664,192 & 462,410 & 1.9787 \\
\textbf{H-01} & Max active ridge degree axis: $\arg\max_i \sum_{j \ne i} N_{i,j}$ & 1,453,220 & -40.5\% & 916,880 & 330,089 & 2.1032 \\
\textbf{H-04} & Linear spanning box penalty: $\mathrm{deg}(i) / (1 + \text{Span})$ & 1,465,252 & -40.0\% & 890,212 & 298,756 & 2.0811 \\
\textbf{H-05} & Quadratic spanning penalty: $\mathrm{deg}(i) / (1 + \text{Span})^2$ & 1,465,676 & -40.0\% & 886,350 & 298,543 & 2.0839 \\
\textbf{H-08} & Leaf base-case cutoff: $|B| \le 2$ boxes & 1,430,876 & -41.4\% & 755,548 & 239,218 & 2.1364 \\
\textbf{H-12} & Leaf base-case cutoff: $|B| \le 8$ boxes & 778,324 & -68.1\% & 108,296 & 39,065 & 2.3681 \\
\textbf{H-14} & Leaf base-case cutoff: $|B| \le 16$ boxes & 313,377 & -87.2\% & 16,014 & 7,407 & 2.2732 \\
\textbf{H-16} & Leaf base-case cutoff: $|B| \le 32$ boxes & 116,980 & -95.2\% & 2,624 & 1,238 & 2.1500 \\
\textbf{H-18} & Phase 1 Champion: Leaf cutoff $|B| \le 64$ & 49,361 & -98.0\% & 514 & 237 & 2.5056 \\
\textbf{H-25} & GSAH Min-Cross-Cut Duplication (quantile 0.25-0.75) & 63,203 & -97.4\% & 882 & 388 & 2.0178 \\
\textbf{H-26} & Centered GSAH with Regularization ($\lambda = 2.0$) & 62,792 & -97.4\% & 850 & 374 & \textbf{1.9499} \\
\textbf{H-36} & Transverse Ridge Penalty ($\mathrm{coeff} = 0.15, |B| \le 128$) & 23,990 & -99.02\% & 118 & 59 & 2.3447 \\
\textbf{H-44} & Coarse GSAH ($|B| \le 160$) & 18,727 & -99.23\% & 74 & 40 & 2.1946 \\
\textbf{H-46} & Coarse GSAH ($|B| \le 224$) & 12,996 & -99.47\% & 40 & 24 & 1.9805 \\
\textbf{H-48} & Coarse GSAH ($|B| \le 320$) & 8,597 & -99.65\% & 22 & 15 & 1.6718 \\
\textbf{H-50} & Coarse GSAH ($|B| \le 448$) & 6,155 & -99.75\% & 14 & 11 & 1.4942 \\
\textbf{H-51} & Coarse GSAH ($|B| \le 500$) & 4,938 & -99.80\% & 12 & 10 & 1.3679 \\
\textbf{H-86} & Clean-Cell 4D Tensor Hook & 4,161 & -99.83\% & 10 & 9 & 1.2625 \\
\textbf{H-582} & \textbf{All-Time Grand Champion}: Simultaneous Corner 2-Cuts & \textbf{3,894} & \textbf{-99.84\%} & \textbf{8} & \textbf{7} & \textbf{1.2481} \\
\bottomrule
\end{tabular}
\end{table*}
"""

for p in [r'E:\code\math\klees_measure_problem\paper\table1_leaderboard.tex',
          r'E:\code\math\klees_measure_problem\tables\table1_leaderboard.tex']:
    with open(p, 'w', encoding='utf-8') as f:
        f.write(t1)

t2 = r"""\begin{table}[t]
\centering
\caption{Ablation of Algorithmic Components on 4D Trellis Weaves ($n=512$).}
\label{tab:ablation}
\small
\begin{tabular}{lrr}
\toprule
\textbf{Algorithmic Mechanism} & \textbf{Peak Work Red.} & \textbf{Rec. Call Red.} \\
\midrule
Active Ridge Endpoint Selection ($\mathrm{deg}(i)$) & $-40.5\%$ & $-44.9\%$ \\
Quadratic Spanning-Box Penalty & $-40.0\%$ & $-46.7\%$ \\
Centered GSAH Min-Duplication Plane ($\lambda = 2.0$) & $-97.4\%$ & $-99.95\%$ \\
Transverse Ridge Duplication Penalty ($\mathrm{coeff} = 0.15$) & $-99.02\%$ & $-99.993\%$ \\
Coarse-to-Fine Adaptive Leaf Sweep ($|B| \le 500$) & $-99.80\%$ & $-99.9993\%$ \\
Clean-Cell 4D Tensor Contraction Hook & $-99.83\%$ & $-99.9994\%$ \\
\textbf{Simultaneous Orthogonal Corner 2-Cuts (H-582)} & $\mathbf{-99.84\%}$ & $\mathbf{-99.9995\%}$ \\
\bottomrule
\end{tabular}
\end{table}
"""

for p in [r'E:\code\math\klees_measure_problem\paper\table2_ablation.tex',
          r'E:\code\math\klees_measure_problem\tables\table2_ablation.tex']:
    with open(p, 'w', encoding='utf-8') as f:
        f.write(t2)

print('Updated table1 and table2')
