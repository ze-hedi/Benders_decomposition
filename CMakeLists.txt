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

option(BUILD_MPI "Build the MPI implementation of Benders" ON)
if(BUILD_MPI)
    find_package(MPI REQUIRED COMPONENTS CXX)
    # OR-Tools exposes Boost 1.87 headers, which has no MPI component: build Boost.MPI from
    # the same version so headers and libraries match.
    set(BOOST_INCLUDE_LIBRARIES mpi serialization)
    set(BOOST_ENABLE_MPI ON)
    FetchContent_Declare(Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.xz
        URL_HASH SHA256=7da75f171837577a52bbf217e17f8ea576c7c246e4594d617bfde7fafd408be5
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(Boost)
endif()

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
    src/utilities/output/JsonWriter.cpp
    src/utilities/output/VoidWriter.cpp)
target_link_libraries(output PUBLIC interfaces jsoncpp_static)
target_compile_definitions(output PRIVATE PROJECT_VER="${PROJECT_VERSION}")

add_library(benders_managers
    src/utilities/benders_managers_utilities/common.cpp
    src/benders_managers/BendersMasterManager.cpp
    src/benders_managers/BendersCutsManagerSequential.cpp
    src/utilities/benders_managers_utilities/BendersMathLogger.cpp
    src/benders_managers/BendersOutputManager.cpp
    src/utilities/benders_managers_utilities/BendersStructsDatas.cpp
    src/utilities/benders_managers_utilities/LastIterationPrinter.cpp
    src/utilities/benders_managers_utilities/LastIterationReader.cpp
    src/utilities/benders_managers_utilities/LastIterationWriter.cpp
    src/utilities/benders_managers_utilities/SubproblemBasisCache.cpp)
target_link_libraries(benders_managers PUBLIC linear_solvers workers interfaces output jsoncpp_static)

add_library(options
    src/utilities/options/OptionsReader.cpp
    src/utilities/options/StructureReader.cpp
    src/utilities/options/WeightsReader.cpp
    src/utilities/options/BendersOptionsManager.cpp)
target_link_libraries(options PUBLIC interfaces jsoncpp_static)

add_library(logger
    src/utilities/logger/CandidateLog.cpp
    src/utilities/logger/IterationResultLog.cpp
    src/utilities/logger/Master.cpp
    src/utilities/logger/MathLogger.cpp
    src/utilities/logger/User.cpp
    src/utilities/logger/UserFile.cpp)
target_link_libraries(logger PUBLIC interfaces benders_managers)

add_library(implementations
    src/implementations/BendersBase.cpp
    src/implementations/BendersSequential.cpp)
target_link_libraries(implementations PUBLIC benders_managers logger output)

if(BUILD_MPI)
    # Interface target until the MPI sources land
    add_library(benders_mpi INTERFACE)
    target_link_libraries(benders_mpi INTERFACE
        implementations Boost::mpi Boost::serialization MPI::MPI_CXX)
endif()

add_library(benders_builder src/implementations/BendersSequentialBuilder.cpp)
target_link_libraries(benders_builder PUBLIC options implementations)

add_executable(benders src/main.cpp)
target_link_libraries(benders PRIVATE benders_builder)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
