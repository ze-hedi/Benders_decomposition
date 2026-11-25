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

add_executable(test_workers
        workers/test_cut.cpp
        workers/test_worker.cpp
        workers/test_subproblem_worker.cpp
        workers/test_worker_master.cpp
)

target_link_libraries(test_workers PRIVATE
        workers
        GTest::gtest_main
)

target_compile_definitions(test_workers PRIVATE
        TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
)

add_test(NAME test_workers COMMAND test_workers)
set_property(TEST test_workers PROPERTY LABELS unit)

add_executable(test_interfaces
        interfaces/test_interfaces.cpp
)

target_link_libraries(test_interfaces PRIVATE
        interfaces
        GTest::gtest_main
)

add_test(NAME test_interfaces COMMAND test_interfaces)
set_property(TEST test_interfaces PROPERTY LABELS unit)

add_executable(test_benders_managers
        benders_managers/test_common.cpp
        benders_managers/test_benders_structs_datas.cpp
)

target_link_libraries(test_benders_managers PRIVATE
        benders_managers
        GTest::gtest_main
)

add_test(NAME test_benders_managers COMMAND test_benders_managers)
set_property(TEST test_benders_managers PROPERTY LABELS unit)
