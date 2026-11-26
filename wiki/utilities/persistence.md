# Persistence

**Summary:** Three classes read, write, and print the `last_iteration` JSON file used to resume
an interrupted run.

**Main code:** `src/include/utilities/benders_managers_utilities/LastIterationReader.h`, `src/utilities/benders_managers_utilities/LastIterationReader.cpp`, `src/include/utilities/benders_managers_utilities/LastIterationWriter.h`, `src/utilities/benders_managers_utilities/LastIterationWriter.cpp`, `src/include/utilities/benders_managers_utilities/LastIterationPrinter.h`, `src/utilities/benders_managers_utilities/LastIterationPrinter.cpp`

**Parent:** [Utilities](index.md)
**Related:** [Output manager](../benders-managers/output-manager.md), [Data structures](data-structures.md)

## Purpose

Resume mode needs the last and best iteration state from a previous run. These classes
serialize and deserialize that state as JSON.

## How it works

### Reader

`LastIterationReader` loads the JSON via `get_json_file_content()` (from `common.h`).
`IsLastIterationFileValid()` is true when the JSON parsed to a non-null value.
`LastIterationData()` returns `(last, best)` by calling `GetIterationData()` for `"last"` and
`"best_iteration"`. `GetIterationData()` reads the scalar fields and the `candidates` array into
a `LogData`.

### Writer

`LastIterationWriter::SaveBestAndLastIterations()` fills two JSON objects (`"last"` and
`"best_iteration"`) and writes them to the file. `FillOutput()` copies each `LogData` field and
the candidates (name, invest, min, max) into the JSON tree.

### Printer

`LastIterationPrinter::Print()` logs a restart summary through the user logger: restart message,
elapsed time, iteration count, best iteration, and best-iteration details.

## Key components

| Name | Location | Role |
|---|---|---|
| `LastIterationReader` | `LastIterationReader.h` / `.cpp` | Reads resume JSON |
| `LastIterationWriter` | `LastIterationWriter.h` / `.cpp` | Writes resume JSON |
| `LastIterationPrinter` | `LastIterationPrinter.h` / `.cpp` | Prints the restart summary |

## Edge cases and unusual behavior

- An invalid JSON file returns a null `Json::Value`, which the reader reports as invalid instead
  of throwing; the output manager then falls back to current in-memory data.
- The writer writes `master_time`/`subproblem_time` from `LogData` but reads them back as
  `master_duration`/`subproblem_duration`, matching the JSON field names.
