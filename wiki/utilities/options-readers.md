# Options readers

**Summary:** The study-input subsystem reads and validates a study folder:
`options.json` (`OptionsReader`), `structure.txt` (`StructureReader`), and an optional weights
file (`WeightsReader`). `BendersOptionsManager` ties them together.

**Main code:** `src/include/utilities/options/BendersOptionsManager.h`, `src/utilities/options/BendersOptionsManager.cpp`, `src/include/utilities/options/OptionsReader.h`, `src/utilities/options/OptionsReader.cpp`, `src/include/utilities/options/StructureReader.h`, `src/utilities/options/StructureReader.cpp`, `src/include/utilities/options/WeightsReader.h`, `src/utilities/options/WeightsReader.cpp`

**Parent:** [Utilities](index.md)
**Related:** [Configuration](../configuration.md), [Builder](../implementations/builder.md)

## Purpose

Before the code relied on the caller to populate `BendersBaseOptions` and the coupling map.
Now a single study folder provides all three inputs, and every file is read and validated up
front so an invalid study fails before anything is built.

## How it works

`BendersOptionsManager` is constructed from an `input_root` directory. It:

1. Throws `OptionsError` if `input_root` is not a directory.
2. Reads `input_root/options.json` via `ReadOptionsFile()` and stores the options and warnings.
3. Reads `input_root/structure.txt` via `ReadStructure()` to build the coupling map, using
   `options_.MASTER_NAME`.
4. When `SLAVE_WEIGHT` is neither `"CONSTANT"` nor `"UNIFORM"`, treats it as a file name and
   reads the weights from `input_root/<SLAVE_WEIGHT>` via `ReadWeights()`.

The result is exposed through `InputRoot()`, `Options()`, `Coupling()`, and `Warnings()`.

## `OptionsReader`

`ReadOptionsFile()` parses the JSON and forwards to `ReadOptions()`. The file must be a JSON
object. `ReadOptions()` walks the keys and applies a setter per accepted key.

Each setter validates type and range and writes into `BendersBaseOptions`. Examples: numeric
gaps must be `>= 0`, `SEPARATION_PARAM` must be in `(0, 1]`, `MAX_ITERATIONS` must be `-1` or
positive, and enums are matched case-insensitively. `RESUME` (boolean) is accepted as the legacy
name of `RESTART_MODE` when the new key is absent. The legacy `SOLVER_NAME` key sets each of
`MASTER_SOLVER` and `SUBPROBLEM_SOLVER` that is not given by its own key.

Unknown keys throw `OptionsError` with a "did you mean" suggestion computed by edit distance.
A fixed `LEGACY_KEYS` set is ignored and added to `warnings` instead. `ReadOptions()` then sets
`INPUTROOT` to the study folder and resolves `OUTPUTROOT` (default `output`, relative paths
under `INPUTROOT`).

After the keys are applied, `CheckSolvers()` validates the solver choices against their roles:
an `INTEGER` master needs a `MASTER_SOLVER` that solves MIPs, and the `SUBPROBLEM_SOLVER` must
give LP duals and a basis (see [Configuration](../configuration.md) for the exact rules and the
`GSCIP` warning).

## `StructureReader`

`ReadStructure()` reads `structure.txt`, one `problem variable column_id` entry per line.
Blank lines and lines starting with `#` are skipped. Each line must have exactly three
whitespace-separated fields, the column id must be a non-negative integer, and a repeated
`(problem, variable)` pair is an error. The result is a `CouplingMap`. A missing entry for the
master is an error.

## `WeightsReader`

`ReadWeights()` reads a weights file, one `name weight` entry per line plus a mandatory
`WEIGHT_SUM sum` line. It checks that every named problem is a subproblem in the coupling map,
weights are non-negative, names are unique, `WEIGHT_SUM` is positive and present, and every
subproblem has a weight. At the end it divides every weight by the sum.

## Key components

| Name | Location | Role |
|---|---|---|
| `BendersOptionsManager` | `BendersOptionsManager.h` / `.cpp` | Reads and validates the whole study folder |
| `OptionsError` | `OptionsReader.h` | Base error for invalid options |
| `ReadOptions` / `ReadOptionsFile` | `OptionsReader.h` / `.cpp` | Parse and validate `options.json` |
| `StructureError` / `ReadStructure` | `StructureReader.h` / `.cpp` | Parse and validate `structure.txt` |
| `WeightsError` / `ReadWeights` | `WeightsReader.h` / `.cpp` | Parse and normalize a weights file |

## Edge cases and unusual behavior

- `ReadBool()` also accepts `0`/`1` integers, because xpansion files sometimes write booleans
  that way.
- `ReadRestartMode()` accepts a legacy boolean as well as the three enum strings.
- Unknown keys fail the whole read; legacy keys never fail, they only warn.
