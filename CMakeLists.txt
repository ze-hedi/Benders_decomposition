cmake_minimum_required(VERSION 3.25)
project(Benders_decomposition VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ortools CONFIG REQUIRED)

add_executable(mathopt_smoke src/main.cpp)
target_link_libraries(mathopt_smoke PRIVATE ortools::ortools)
