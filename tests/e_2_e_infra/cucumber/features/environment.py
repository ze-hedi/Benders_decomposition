"""Behave hooks: one fresh temporary directory per scenario.

On failure the directory is kept and its path printed so the study and the
outputs can be inspected manually.
"""

import shutil
import tempfile
from pathlib import Path


def before_scenario(context, scenario):
    context.temp_dir = Path(tempfile.mkdtemp(prefix="benders_e2e_"))


def after_scenario(context, scenario):
    if scenario.status == "failed":
        print(f"Study directory kept for debugging: {context.temp_dir}")
    else:
        shutil.rmtree(context.temp_dir, ignore_errors=True)
