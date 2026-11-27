# Glossary

**Summary:** Domain terms used across the Benders decomposition code.

**Parent:** [Entry](entry.md)
**Related:** [Architecture](architecture.md), [Configuration](configuration.md)

## Benders decomposition

An algorithm for problems of the form "choose some *first-stage* variables, then pay a cost that
depends on them". The master chooses the first-stage variables; the subproblems compute the
cost of those choices. The master receives *cuts* that under-approximate that cost and is solved
again, until the gap between a lower bound and an upper bound is small enough.

- **Master problem** — the problem that owns the investment/linking decisions plus the `alpha`
  variables that under-approximate the subproblem costs.
- **Subproblem** — one operational problem solved at a fixed master candidate. Its optimal value
  and dual information produce a cut.
- **Linking variable** — a variable that appears in both the master and the subproblems, passed
  from master to subproblems by fixing its bounds.
- **Candidate (`x_cut`)** — the master solution, possibly blended by the separation parameter,
  at which the subproblems are solved.
- **Separation parameter** — a value in `[0,1]` blending the previous and current master
  candidates into `x_cut`; `1` means use the current candidate only.
- **Subgradient** — the reduced costs of the linking variables of a subproblem at `x_cut`; they
  become the cut coefficients. See [Subproblem solver](solvers/subproblem-solver.md).
- **Cut** — a row added to the master of the form
  `sum(alpha_i) >= cost + subgradient . (x - x_cut)`. It keeps the master's `alpha` variables
  honest. See [Cut building](solvers/cut-building.md).
- **Alpha variable** — a master variable bounding the subproblem cost. `alpha_i` bounds one
  subproblem; the global `alpha` equals their sum and carries the objective coefficient.
- **Upper bound / lower bound** — the best candidate cost found so far (`ub`) and the master's
  current optimal value (`lb`); the algorithm stops when they are close.
- **Master formulation** — whether the master is solved as an integer (`INTEGER`) or LP-relaxed
  (`RELAXED`) problem.
- **LP relaxation** — solving an integer master with its integrality constraints removed, used as
  an initial phase or the whole run.

## Execution modes

- **Cold start** — start from scratch; create the alpha variables.
- **Resume** — continue an interrupted run by loading `last_iteration` JSON state.
- **Hot start** — restart but reuse a master MPS that already contains the alpha columns.
- **Cache problems** — `NO_CACHE` keeps subproblem solvers in memory; `PER_SUB` rebuilds them
  from disk each iteration and restores a stored basis.
- **Algorithm driver** — the object that runs the algorithm: `BendersSequential` in one process
  (`ALGORITHM = SEQUENTIAL`) or `BendersMpi` over MPI ranks (`ALGORITHM = MPI`). See
  [Implementations](implementations/index.md).
- **Warm start** — starting a solve from a previously saved basis, done for recreated
  subproblems via `SubproblemBasisCache`.

## Solver terms

- **MathOpt** — Google OR-Tools' solver-agnostic modeling and solve API, the backend used here.
- **MPS** — a text format for linear/integer problems, used for all problem input/output.
- **Basis** — the simplex basis status of rows and columns, used to warm start an LP.
- **Dual / reduced cost** — dual values of the constraints and reduced costs of the variables of
  an LP solve; reduced costs of linking variables are the subgradient.

## Other terms

- **Coupling map** — `CouplingMap` maps each problem name (including the master) to its linking
  variables. See [Data structures](utilities/data-structures.md).
- **`alpha` fixing constraints** — equality rows that tie `alpha_0` to the other alphas of a
  group that shares one cut.
- **Aggregation** — grouping several subproblems into a single cut, controlled by
  `NB_CUTS_PER_ITER`.
