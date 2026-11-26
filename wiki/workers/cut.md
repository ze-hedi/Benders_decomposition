# Cut

**Summary:** `Cut` holds one Benders optimality cut as a master row, and `make_cut()` builds it
from a subproblem subgradient, the candidate, and the subproblem cost.

**Main code:** `src/include/workers/Cut.h`, `src/workers/Cut.cpp`, `src/include/workers/types.h`

**Parent:** [Workers](index.md)
**Related:** [Subproblem worker](subproblem-worker.md), [Worker master](worker-master.md), [Cuts manager](../benders-managers/cuts-manager.md)

## Purpose

The master's `alpha` variables under-approximate the subproblem cost. A cut is the linear
constraint that keeps that approximation valid at the current candidate.

## How it works

`make_cut()` builds the inequality:

```text
sum(alpha_i) >= subproblem_cost + subgradient . (x - x_cut)
```

It is stored in the master as an `L` row by rewriting it as:

```text
subgradient . x - sum(alpha_i) <= subgradient . x_cut - subproblem_cost
```

The implementation:

1. Starts `rhs = -subproblem_cost`.
2. For each linking variable that also appears in the subgradient, pushes its master column and
   the subgradient coefficient, and adds `subgradient * x_cut` to `rhs`. Linking-variable names
   absent from the subgradient are skipped.
3. For each alpha column, pushes the column with coefficient `-1`.
4. Rounds the rhs and every coefficient to zero when `abs(value) < tolerance`.

`x_cut.at(name)` throws `std::out_of_range` if a linking variable present in the subgradient is
missing from the candidate point.

## Key components

| Name | Location | Role |
|---|---|---|
| `Cut` | `Cut.h` | `columns`, `coefficients`, `rhs` of one master row |
| `make_cut()` | `Cut.cpp` | Builds the cut and rounds it |

## Edge cases and unusual behavior

- The tolerance is expected to stay below `1`, so the `-1` alpha coefficients are never rounded
  away (noted in `Cut.cpp`).
- A subgradient name that is not a linking variable is ignored rather than erroring.
