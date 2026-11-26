find_package(GTest CONFIG REQUIRED)

# Registers an already defined test executable to run on np MPI processes
function(add_mpi_test name np)
    add_test(NAME ${name}_np${np}
             COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${np} ${MPIEXEC_PREFLAGS}
                     $<TARGET_FILE:${name}> ${MPIEXEC_POSTFLAGS})
    set_property(TEST ${name}_np${np} PROPERTY LABELS mpi)
    set_property(TEST ${name}_np${np} PROPERTY PROCESSORS ${np})
endfunction()

add_subdirectory(unit_tests)
