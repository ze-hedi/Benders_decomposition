cmake_minimum_required(VERSION 3.25)
project(Benders_decomposition VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ortools CONFIG REQUIRED)

include(FetchContent)
set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_OBJECT_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG 1.9.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(jsoncpp)

add_library(interfaces INTERFACE)
target_include_directories(interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/include)

add_library(linear_solvers src/solvers/LinearSolvers.cpp)
target_include_directories(linear_solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
target_link_libraries(linear_solvers PUBLIC ortools::ortools)

add_library(workers
    src/workers/Cut.cpp
    src/workers/Worker.cpp
    src/workers/SubproblemWorker.cpp
    src/workers/WorkerMaster.cpp)
target_link_libraries(workers PUBLIC linear_solvers)

add_library(output
    src/output/JsonWriter.cpp
    src/output/VoidWriter.cpp)
target_link_libraries(output PUBLIC interfaces jsoncpp_static)
target_compile_definitions(output PRIVATE PROJECT_VER="${PROJECT_VERSION}")

add_library(benders_managers
    src/benders_managers/common.cpp
    src/benders_managers/BendersMathLogger.cpp
    src/benders_managers/BendersStructsDatas.cpp
    src/benders_managers/LastIterationPrinter.cpp
    src/benders_managers/LastIterationReader.cpp
    src/benders_managers/LastIterationWriter.cpp)
target_link_libraries(benders_managers PUBLIC linear_solvers interfaces jsoncpp_static)

add_library(logger
    src/logger/CandidateLog.cpp
    src/logger/IterationResultLog.cpp
    src/logger/Master.cpp
    src/logger/MathLogger.cpp
    src/logger/User.cpp
    src/logger/UserFile.cpp)
target_link_libraries(logger PUBLIC interfaces benders_managers)

add_executable(mathopt_smoke src/main.cpp)
target_link_libraries(mathopt_smoke PRIVATE ortools::ortools)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
