add_executable(test_linear_solvers
        solvers/test_linear_solvers.cpp
        solvers/test_linear_solvers_getters.cpp
)

target_link_libraries(test_linear_solvers PRIVATE
        linear_solvers
        GTest::gtest_main
)

target_compile_definitions(test_linear_solvers PRIVATE
        TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
)

add_test(NAME test_linear_solvers COMMAND test_linear_solvers)
set_property(TEST test_linear_solvers PROPERTY LABELS unit)
