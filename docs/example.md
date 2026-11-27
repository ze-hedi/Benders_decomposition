# Example

This page walks through a complete study: a toy investment problem split into a master and two
subproblems, then the run and its outputs. The study is
`tests/e_2_e_infra/data_test/mini_instance_LP` in the repository.

## The problem

We invest in a capacity $x$, which costs $1.5$ per unit and is at most $10$. Two scenarios each
need a demand to be met: $1.5$ in the first one and $2.5$ in the second one. Any demand that the
capacity does not cover is lost, at a cost of $1$ per unit:

$$
\begin{aligned}
\min_{x,\,z_1,\,z_2} \quad & 1.5\,x + z_1 + z_2 \\
\text{s.t.} \quad & x + z_1 \ge 1.5 \\
& x + z_2 \ge 2.5 \\
& 0 \le x \le 10, \quad z_1, z_2 \ge 0
\end{aligned}
$$

$x$ is the investment: it goes to the master. $z_1$ and $z_2$ are the operation of each scenario:
each one goes to its subproblem, where $x$ is a fixed value.

## The study folder

```
mini_instance_LP/
├── options.json      # how to run
├── structure.txt     # which variables link the master and the subproblems
├── master.mps        # the master problem
├── SP1.mps           # one MPS file per subproblem
└── SP2.mps
```

### The master

`master.mps` holds the investment variable and its cost. The cost of the subproblems is added by
Benders itself, through one `alpha` variable per subproblem.

```
NAME          master
ROWS
 N        OBJ
COLUMNS
    x     OBJ       1.5
RHS
BOUNDS
    UP    BND       x       10.0
    LO    BND       x       0.0
ENDATA
```

### The subproblems

Each subproblem has its own copy of $x$, which Benders fixes to the master's value at each
iteration. `SP1.mps` is the first scenario, `SP2.mps` is the same with a demand of `2.5`:

```
NAME          SP1
ROWS
 N    OBJ
 G    C1
COLUMNS
    x       C1      1
    perte   OBJ     1
    perte   C1      1
RHS
    RHS     C1      1.5
BOUNDS
    LO      BND     perte    0.0
ENDATA
```

### The structure

`structure.txt` lists, for each problem, the linking variables and their column index in that
problem. A variable with the same name in the master and in a subproblem is the same decision:

```
master      x   0
SP1.mps     x   0
SP2.mps     x   0
```

The master's line uses `MASTER_NAME` (`master` by default). The subproblem names are their MPS
file names in the study folder.

### The options

`options.json` configures the run. Every option is optional, see [Options](options.md):

```json
{
    "LOG_LEVEL": 1,
    "MASTER_SOLVER": "HIGHS",
    "SUBPROBLEM_SOLVER": "HIGHS",
    "SEPARATION_PARAM": 1
}
```

## Running it

The only argument is the study folder:

```sh
benders mini_instance_LP
```

To spread the subproblems over MPI processes, set `"ALGORITHM": "MPI"` in `options.json` and
launch it with `mpirun`:

```sh
mpirun -np 2 benders mini_instance_LP
```

With `LOG_LEVEL` above `0`, each iteration is printed on the console:

```
ITERATION 1:
	Solving master...
		Candidates:
			x = 0.00 invested MW -- possible interval [0.00; 10.00] MW
	Solving subproblems...
ITERATION 2:
		Candidates:
			x = 10.00 invested MW -- possible interval [0.00; 10.00] MW
ITERATION 3:
		Candidates:
			x = 1.50 invested MW -- possible interval [0.00; 10.00] MW
--- Run completed: absolute gap reached
	Total number of iterations done = 3
	Best solution = it 3
```

The first master knows nothing about the subproblems and invests nothing. Each iteration adds one
cut per subproblem, and by the third one the master has found the optimum. The console prints the
costs in millions, so on this toy problem they all round to `0.00 Me`: read the results in
`out.json`.

## The outputs

The outputs are written in `OUTPUTROOT`, `output/` in the study folder by default:

| File | Content |
|------|---------|
| `out.json` | The options, every iteration and the best solution |
| `benders_output_trace.csv` | One line per problem and iteration: bounds, simplex iterations, times |
| `reportbenders.txt` | The log of the run, as printed on the console |
| `benders_solver.log` | The detailed log |
| `last_iteration.json` | The state of the last and best iterations, to resume the run |
| `master_last_iteration.mps` | The master with all its cuts, to restart from it |
| `master_last_basis` | The basis of the last master |

The `solution` section of `out.json` gives the result:

```json
"solution": {
    "investment_cost": 2.25,
    "iteration": 3,
    "operational_cost": 1.0,
    "optimality_gap": 0.0,
    "overall_cost": 3.25,
    "problem_status": "OPTIMAL",
    "relative_gap": 0.0,
    "stopping_criterion": "absolute gap",
    "values": {
        "x": 1.5
    }
}
```

We invest $x = 1.5$, which covers the first demand: only $1$ unit of the second one is lost. The
overall cost is $1.5 \times 1.5 + 1 = 3.25$.
