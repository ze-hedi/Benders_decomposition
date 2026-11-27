"""Given steps: study setup and options mutation."""

import shutil

from behave import given
from steps import update_option
from utils_functions import resolve_study_path


@given('the study path is "{study}"')
def study_path_is(context, study):
    """Copy a reference study from data_test/ into the scenario temp dir so
    that mutations and outputs never touch the repository."""
    context.study_path = resolve_study_path(study)
    if not context.study_path.is_dir():
        raise AssertionError(f"Study directory not found: {context.study_path}")
    context.tmp_study = context.temp_dir / context.study_path.name
    shutil.copytree(context.study_path, context.tmp_study)


@given('solver is "{solver}"')
def set_solver(context, solver):
    update_option(context.tmp_study, "MASTER_SOLVER", solver)
    update_option(context.tmp_study, "SUBPROBLEM_SOLVER", solver)


@given("the number of cuts per iteration is {n:d}")
def set_nb_cuts_per_iter(context, n):
    update_option(context.tmp_study, "NB_CUTS_PER_ITER", n)


@given("the cache problems level is {level}")
def set_cache_problems_level(context, level):
    update_option(context.tmp_study, "CACHE_PROBLEMS", level)
