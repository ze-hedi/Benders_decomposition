"""Shared helpers for the cucumber end-to-end steps.

Adapted from antares-xpansion tests/end_to_end/utils_functions.py.
"""

import json
import os
from pathlib import Path

# tests/e_2_e_infra
INFRA_ROOT = Path(os.path.abspath(__file__)).parent
# repository root
REPO_ROOT = INFRA_ROOT.parent.parent


def get_benders_executable() -> Path:
    """E2E_BENDERS if set, else <repo>/build/benders."""
    return Path(os.environ.get("E2E_BENDERS") or REPO_ROOT / "build" / "benders")


def get_mpi_command(nproc: int = 1):
    """MPI launcher prefix. Empty for a single process, which runs without
    mpirun."""
    if nproc <= 1:
        return []
    return ["mpirun", "-np", str(nproc), "--oversubscribe"]


def read_json_file(json_path) -> dict:
    with open(json_path, "r") as file:
        return json.load(file)


def write_json_file(json_path, content) -> None:
    with open(json_path, "w") as file:
        json.dump(content, file, indent=4)


def resolve_study_path(study: str) -> Path:
    """Resolve a study path written in a feature file.

    Relative paths are resolved from the infra root (tests/e_2_e_infra), e.g.
    "data_test/mini_instance_LP" -> tests/e_2_e_infra/data_test/mini_instance_LP.
    """
    path = Path(study)
    return path if path.is_absolute() else INFRA_ROOT / path
