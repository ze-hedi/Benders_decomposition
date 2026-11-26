"""Then steps: assertions on return code, CLI output and result JSON.

Expected numerical values are compared with numpy allclose (rtol 1e-6),
same contract as antares-xpansion. The result JSON schema expected in
context.outputs is documented in tests/e_2_e_infra/README.md.
"""

import numpy as np
from behave import then


def assert_dict_allclose(actual, expected, rtol=1e-06, atol=0):
    for key in expected:
        assert key in actual, f"Variable '{key}' missing from the solution values"
        np.testing.assert_allclose(
            actual[key],
            expected[key],
            rtol=rtol,
            atol=atol,
            err_msg=f"Mismatch found at key '{key}'",
        )


def assert_outputs_available(context):
    assert context.outputs is not None, (
        "No result JSON available (check JSON_FILE in options.json and the run return code)"
    )


@then("the return status is {code:d}")
def check_return_status(context, code):
    assert context.return_code == code, (
        f"Expected return code {code}, got {context.return_code}"
    )


@then("the simulation succeeds")
def simulation_succeeds(context):
    assert context.return_code == 0, (
        f"Expected return code 0, got {context.return_code}"
    )


@then("the simulation takes less than {seconds:d} seconds")
def check_simulation_time(context, seconds):
    assert_outputs_available(context)
    assert context.outputs["run_duration"] <= seconds


@then("the expected overall cost is {value:g}")
def check_overall_cost(context, value):
    assert_outputs_available(context)
    actual = context.outputs["solution"]["overall_cost"]
    print(f".... overall cost : {actual}")
    np.testing.assert_allclose(actual, value, rtol=1e-6, atol=0)


@then("the expected investment cost is {value:g}")
def check_investment_cost(context, value):
    assert_outputs_available(context)
    actual = context.outputs["solution"]["investment_cost"]
    np.testing.assert_allclose(actual, value, rtol=1e-6, atol=0)


@then("the expected operational cost is {value:g}")
def check_operational_cost(context, value):
    assert_outputs_available(context)
    actual = context.outputs["solution"]["operational_cost"]
    np.testing.assert_allclose(actual, value, rtol=1e-6, atol=0)


@then('the problem_status is "{problem_status}"')
def check_problem_status(context, problem_status):
    assert_outputs_available(context)
    actual = context.outputs["solution"]["problem_status"]
    assert actual == problem_status, (
        f"Expected problem_status '{problem_status}', got '{actual}'"
    )


@then("the number of iterations is {n:d}")
def check_number_of_iterations(context, n):
    assert_outputs_available(context)
    actual = len(context.outputs["iterations"])
    assert actual == n, f"Expected {n} iterations, got {actual}"


@then("the solution is")
def check_solution(context):
    """Data table with headers: variable | value"""
    assert_outputs_available(context)
    expected_solution = {row["variable"]: float(row["value"]) for row in context.table}
    assert_dict_allclose(context.outputs["solution"]["values"], expected_solution)


@then('stdout contains "{text}"')
def check_stdout_contains(context, text):
    assert text in context.stdout, f"'{text}' not found in stdout:\n{context.stdout}"


@then('stderr contains "{text}"')
def check_stderr_contains(context, text):
    assert text in context.stderr, f"'{text}' not found in stderr:\n{context.stderr}"
