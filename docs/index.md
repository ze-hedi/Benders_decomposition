# Benders decomposition

A C++20 implementation of Benders decomposition for two-stage problems: a master problem (MILP or
LP) over investment variables, and LP subproblems that evaluate each investment. It is built on
[OR-Tools MathOpt](https://developers.google.com/optimization/math_opt), so the master and the
subproblems can each use any MathOpt solver.

Two drivers share the same algorithm, chosen with the `ALGORITHM` option:

- **`SEQUENTIAL`**: the master and every subproblem in one process.
- **`MPI`**: rank 0 owns the master, the subproblems are spread over all ranks.

## Running a study

```sh
benders <study_folder>                       # ALGORITHM = SEQUENTIAL
mpirun -np 4 benders <study_folder>          # ALGORITHM = MPI
```
