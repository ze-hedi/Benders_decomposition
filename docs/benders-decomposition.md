# Benders Decomposition

## Subproblem

For a fixed master decision $\bar{x}$, each subproblem $s \in \mathcal{S}$ solves:

$$
Q_s(\bar{x}) = \min_{y_s \ge 0} \left\{ q_s^\top y_s \;:\; W_s y_s \ge h_s - T_s \bar{x} \right\}
$$

It returns its optimal value $Q_s(\bar{x})$ and the gradient with respect to $x$:

$$
g_s(\bar{x}) = -\,T_s^\top \pi_s^*
$$

where $\pi_s^*$ is the optimal dual.

## Cut

$Q_s$ is convex, so the first-order approximation is a lower bound everywhere:

$$
\theta_s \;\ge\; Q_s(\bar{x}) + g_s(\bar{x})^\top (x - \bar{x})
$$

## Master

$$
\begin{aligned}
\min_{x,\,\theta} \quad & c^\top x + \sum_{s \in \mathcal{S}} \theta_s \\
\text{s.t.} \quad & \theta_s \ge Q_s(\bar{x}^k) + g_s(\bar{x}^k)^\top (x - \bar{x}^k) && \forall s,\; \forall k \\
& x \in \mathcal{X}
\end{aligned}
$$

It returns a new $\bar{x}$, which goes back to the subproblems.

## Aggregated cuts

Instead of one cut per subproblem, we can split $\mathcal{S}$ into groups $G_1, \dots, G_m$ and add one cut per group by summing values and gradients:

$$
\theta_{G} \;\ge\; \sum_{s \in G} Q_s(\bar{x}) + \Big( \sum_{s \in G} g_s(\bar{x}) \Big)^\top (x - \bar{x})
$$

with $\theta_G$ standing in for $\sum_{s \in G} Q_s(x)$ in the master objective.

- $m = |\mathcal{S}|$: multi-cut (one cut per subproblem).
- $m = 1$: single-cut (one cut for all).
- In between: fewer cuts and a smaller master, but a weaker approximation, so convergence can take more iterations.
