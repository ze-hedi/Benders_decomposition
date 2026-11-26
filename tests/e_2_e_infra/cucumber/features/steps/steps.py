"""Helpers shared by the given/when/then step modules.

Given/when/then file organisation mirrors antares-xpansion: split by clause
type to avoid a single huge steps.py file.
"""

from pathlib import Path

from utils_functions import read_json_file, write_json_file


def options_path(study_dir) -> Path:
    return Path(study_dir) / "options.json"


def read_options(study_dir) -> dict:
    return read_json_file(options_path(study_dir))


def update_option(study_dir, key: str, value) -> None:
    """Set a single key in the study options.json."""
    options = read_options(study_dir)
    options[key] = value
    write_json_file(options_path(study_dir), options)
