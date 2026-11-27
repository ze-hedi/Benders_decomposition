"""When steps: run the Benders CLI on a prepared study."""

import os
import shutil
import subprocess
from pathlib import Path

from behave import when
from steps import read_options, update_option
from utils_functions import get_benders_executable, get_mpi_command, read_json_file


def build_benders_command(nproc: int):
    command = get_mpi_command(nproc=nproc)
    command.append(str(get_benders_executable()))
    # The CLI takes the study folder and reads its options.json; cwd is the study
    command.append(".")
    return command


def process_command(context, nproc: int, option_file: str):
    """Run the built command with cwd = temporary study directory, capture
    return code / stdout / stderr and load the JSON output declared in the
    study options (JSON_FILE)."""
    if option_file != "options.json":
        shutil.copy(Path(context.tmp_study) / option_file, Path(context.tmp_study) / "options.json")
    if nproc > 1:
        update_option(context.tmp_study, "ALGORITHM", "MPI")
    command = build_benders_command(nproc)
    print(f"Running command: {' '.join(command)}")
    old_cwd = os.getcwd()
    os.chdir(context.tmp_study)
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = process.communicate()
        context.return_code = process.returncode
        context.stdout = out.replace(b"\r\n", b"\n").decode("utf-8", errors="replace")
        context.stderr = err.replace(b"\r\n", b"\n").decode("utf-8", errors="replace")
        if context.return_code != 0:
            print("*********************** Begin stdout ***********************")
            print(context.stdout)
            print("*********************** End stdout ***********************")
            print("*********************** Begin stderr ***********************")
            print(context.stderr)
            print("*********************** End stderr ***********************")
        options = read_options(context.tmp_study)
        if "JSON_FILE" in options:
            # Resolved against OUTPUTROOT, as the CLI does (default "output")
            output_root = Path(context.tmp_study) / options.get("OUTPUTROOT", "output")
            output_file_path = output_root / options["JSON_FILE"]
            if output_file_path.is_file():
                context.outputs = read_json_file(output_file_path)
            else:
                context.outputs = None
                print(f"Warning: expected output file not found: {output_file_path}")
        else:
            context.outputs = None
    finally:
        os.chdir(old_cwd)


@when("I set MAX_ITERATIONS to {max_iterations:d}")
def set_max_iterations(context, max_iterations):
    update_option(context.tmp_study, "MAX_ITERATIONS", max_iterations)


@when('I set RESTART_MODE to "{resume_mode}"')
def set_resume_mode(context, resume_mode):
    update_option(context.tmp_study, "RESTART_MODE", resume_mode)
    # Replace master.mps with master_last_iteration.mps (which contains the
    # accumulated cuts), as in antares-xpansion
    study_path = Path(context.tmp_study)
    last_master = study_path / "master_last_iteration.mps"
    if last_master.is_file():
        shutil.copy(last_master, study_path / "master.mps")


@when("I run benders")
@when('I run benders with "{option_file}" as option file')
def run_benders(context, option_file: str = "options.json"):
    process_command(context, 1, option_file)


@when("I run benders with {n:d} proc(s)")
@when('I run benders with {n:d} proc(s) and "{option_file}" as option file')
def run_benders_mpi(context, n, option_file: str = "options.json"):
    process_command(context, n, option_file)
