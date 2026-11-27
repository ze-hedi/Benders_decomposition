# End-to-end cucumber infrastructure

Behave-based (Python Cucumber) end-to-end test infrastructure, reproducing the
layout of `antares-xpansion/tests/end_to_end/cucumber`. This directory contains
the framework only: steps, hooks, helpers and configuration. Feature files and
`data_test/` studies are added on top of it.

## Layout

```
tests/e_2_e_infra/
├── requirements.txt          # behave, numpy
├── utils_functions.py        # executable path, MPI prefix, json/study helpers
├── data_test/                # reference studies used by the features
│   └── <study>/
│       ├── options.json      # Benders options (see contract below)
│       ├── master.mps
│       ├── sub/*.mps         # subproblems, as named in structure.txt
│       └── structure.txt     # "<problem> <linking var> <column id>" per line
├── e_2_e_study_cases/        # larger studies, run by hand (not used by the features)
└── cucumber/features/
    ├── *.feature             # Gherkin scenarios
    ├── environment.py        # behave hooks: temp dir per scenario
    └── steps/
        ├── steps.py          # shared helpers (options.json mutation)
        ├── given.py          # study copy + option edits
        ├── when.py           # run the Benders CLI, capture outputs
        └── then.py           # assertions (return code, JSON values, tables)
```

## Running

```bash
pip install -r tests/e_2_e_infra/requirements.txt
cd tests/e_2_e_infra
behave --tags "not @flaky and not @noci" cucumber/features \
    --no-capture --no-capture-stderr --format pretty
```

`behave` must be launched from `tests/e_2_e_infra` (step modules import
`utils_functions` from this directory).

Tag conventions (same as antares-xpansion): `@fast`, `@short` for quick
scenarios; `@flaky` and `@noci` are excluded from CI runs.

## Executable configuration

The steps run `E2E_BENDERS` if it is set, else `<repo>/build/benders`:

```bash
E2E_BENDERS=/path/to/benders behave cucumber/features
```

`When I run benders with N proc(s)` with N > 1 launches `mpirun -np N` and
sets `ALGORITHM` to `MPI` in the copied `options.json`.

## Contracts

### Study `options.json` (input)

Keys mirror `BendersBaseOptions` (see
`src/include/utilities/benders_managers_utilities/common.h`):

| key                | meaning                                   |
|--------------------|-------------------------------------------|
| `OUTPUTROOT`       | output directory (usually `./output/`)    |
| `MASTER_NAME`      | master problem base name                  |
| `MASTER_SOLVER`    | master MIP solver (`highs`, `gscip`, ...) |
| `SUBPROBLEM_SOLVER` | subproblem LP solver (`glop`, `highs`, ...) |
| `MAX_ITERATIONS`   | Benders iteration limit (-1 = unlimited)  |
| `NB_CUTS_PER_ITER` | cuts per iteration (0 = one per sub)      |
| `ALGORITHM`        | `SEQUENTIAL` / `MPI`                      |
| `RESTART_MODE`     | `cold_start` / `resume` / `hot_start`     |
| `SLAVE_WEIGHT`     | `CONSTANT` / `UNIFORM`                    |
| `SLAVE_WEIGHT_VALUE` | weight value                            |
| `JSON_FILE`        | result JSON written by the CLI            |

### Result JSON (`JSON_FILE`, output)

Schema expected by the `then` steps (same shape as xpansion `out.json`):

```json
{
  "run_duration": 0.12,
  "solution": {
    "overall_cost": 5.0,
    "problem_status": "OPTIMAL",
    "values": {"x": 5.0}
  },
  "iterations": [ {"iteration": 1} ]
}
```

### Scenario lifecycle

- `before_scenario`: a fresh temp dir is created (`context.temp_dir`)
- `Given the study path is "data_test/<study>"`: the study is copied into the
  temp dir; all mutations and outputs happen there
- `When I run benders`: the CLI is executed with cwd = copied study, capturing
  `context.return_code/stdout/stderr` and loading `context.outputs`
- `after_scenario`: temp dir removed on success, kept (path printed) on failure

## Studies and features brought from antares-xpansion

Self-contained studies (master/sub MPS + structure.txt + options, runnable by
the Benders CLI alone) copied into `data_test/`, with generated `outputs/`
stripped:

| study | used by | notes |
|---|---|---|
| `Benders_handle_mixed_order_of_var` | `Benders.feature` | mixed master/sub variable order |
| `ieee96_base` | `benders_resume_modes.feature` | resume / hot_start |
| `test_cut_aggregation_ieee96_10inc` | `benders_aggreg_cuts.feature` | cuts per iteration / cache levels / MPI |
| `mini_instance_{LP,MIP,INFEAS,UNBOUNDED}` | (future features) | options file is `options_default.json` |

Compatibility changes vs upstream:

- Solvers are set with `MASTER_SOLVER` and `SUBPROBLEM_SOLVER`, both `highs`
  (upstream used `Xpress`/`COIN`); `SOLVER_NAME` is still accepted and sets both
- `Benders_handle_mixed_order_of_var`: legacy `GAP` renamed to `ABSOLUTE_GAP`
- `benders_aggreg_cuts.feature` sets `NB_CUTS_PER_ITER` (upstream's
  `BATCH_SIZE` is ignored by this port)
- Not ported: micro-iteration studies/features (`ieee96_micro_it*`,
  `micro_it_mem_optim_study` — feature reverted in this repo) and pipeline
  studies (`examples/`, `trajectory`, `sensitivity`, ... — they need lpnamer,
  the Antares simulator and launch.py)
