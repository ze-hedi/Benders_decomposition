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
        utilities/interfaces/test_interfaces.cpp
)

target_link_libraries(test_interfaces PRIVATE
        interfaces
        GTest::gtest_main
)

add_test(NAME test_interfaces COMMAND test_interfaces)
set_property(TEST test_interfaces PROPERTY LABELS unit)

add_executable(test_benders_managers
        utilities/benders_managers_utilities/test_common.cpp
        utilities/benders_managers_utilities/test_benders_structs_datas.cpp
        utilities/benders_managers_utilities/test_last_iteration.cpp
        benders_managers/test_benders_output_manager.cpp
        benders_managers/test_benders_master_manager.cpp
        benders_managers/test_benders_subproblems_manager.cpp
        benders_managers/test_benders_cuts_manager.cpp
        benders_managers/test_benders_loop.cpp
        benders_managers/test_subproblem_constraints_manager.cpp
        utilities/benders_managers_utilities/test_subproblem_basis_cache.cpp
)

target_include_directories(test_benders_managers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(test_benders_managers PRIVATE
        benders_managers
        GTest::gtest_main
)

target_compile_definitions(test_benders_managers PRIVATE
        TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
)

add_test(NAME test_benders_managers COMMAND test_benders_managers)
set_property(TEST test_benders_managers PROPERTY LABELS unit)

add_executable(test_output
        utilities/output/test_json_writer.cpp
        utilities/output/test_void_writer.cpp
)

target_link_libraries(test_output PRIVATE
        output
        GTest::gtest_main
)

add_test(NAME test_output COMMAND test_output)
set_property(TEST test_output PROPERTY LABELS unit)

add_executable(test_logger
        utilities/logger/test_logger.cpp
        utilities/logger/test_math_logger.cpp
)

target_include_directories(test_logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(test_logger PRIVATE
        logger
        GTest::gtest_main
)

add_test(NAME test_logger COMMAND test_logger)
set_property(TEST test_logger PROPERTY LABELS unit)

add_executable(test_implementations
        implementations/test_benders_base.cpp
)

target_include_directories(test_implementations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(test_implementations PRIVATE
        implementations
        GTest::gtest_main
)

add_test(NAME test_implementations COMMAND test_implementations)
set_property(TEST test_implementations PROPERTY LABELS unit)
