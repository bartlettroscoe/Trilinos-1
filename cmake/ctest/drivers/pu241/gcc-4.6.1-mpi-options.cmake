# Primary Stable options for MPI builds with GCC 4.6.1
INCLUDE(${TRILINOS_HOME_DIR}/cmake/ctest/drivers/pu241/gcc-4.6.1-base-options.cmake)
SET(MPI_BASE_DIR "${TRILINOS_TOOLSET_BASE}" CACHE PATH "" FORCE)

# Used only when TriKota/Dakota are enabled with CMake
SET(TriKota_ENABLE_DakotaCMake ON CACHE BOOL "" FORCE)
SET(ENABLE_DAKOTA_TESTS OFF CACHE BOOL "" FORCE)
SET(HAVE_ACRO OFF CACHE BOOL "" FORCE)
SET(HAVE_AMPL OFF CACHE BOOL "" FORCE)
SET(HAVE_X_GRAPHICS OFF CACHE BOOL "" FORCE)
SET(HAVE_HOPSPACK OFF CACHE BOOL "" FORCE)