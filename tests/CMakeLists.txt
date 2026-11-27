find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Registers an already defined test executable to run on np MPI processes
function(add_mpi_test name np)
    add_test(NAME ${name}_np${np}
             COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${np} ${MPIEXEC_PREFLAGS}
                     $<TARGET_FILE:${name}> ${MPIEXEC_POSTFLAGS})
    set_property(TEST ${name}_np${np} PROPERTY LABELS mpi)
    set_property(TEST ${name}_np${np} PROPERTY PROCESSORS ${np})
endfunction()

add_subdirectory(unit_tests)
