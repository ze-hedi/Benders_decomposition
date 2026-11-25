cmake_minimum_required(VERSION 3.25)
project(Benders_decomposition VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ortools CONFIG REQUIRED)

add_library(linear_solvers src/solvers/LinearSolvers.cpp)
target_include_directories(linear_solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
target_link_libraries(linear_solvers PUBLIC ortools::ortools)

add_library(workers
    src/workers/Cut.cpp
    src/workers/Worker.cpp
    src/workers/SubproblemWorker.cpp
    src/workers/WorkerMaster.cpp)
target_link_libraries(workers PUBLIC linear_solvers)

add_executable(mathopt_smoke src/main.cpp)
target_link_libraries(mathopt_smoke PRIVATE ortools::ortools)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
