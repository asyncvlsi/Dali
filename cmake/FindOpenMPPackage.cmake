############################################################################
# Find OpenMP package
#
# Apple's toolchain needs libomp from Homebrew, and needs -fopenmp handed to the
# preprocessor. Both tokens are kept together in OpenMP_CXX_FLAGS: supplying
# -Xpreprocessor on its own relies on it landing immediately before -fopenmp,
# and it otherwise consumes whatever flag happens to follow.
#
# The library is resolved by name rather than by filename, so
# CMAKE_FIND_LIBRARY_SUFFIXES decides whether the shared or static libomp is
# linked. A build wanting the static one sets that preference before including
# this file.
############################################################################
if(APPLE)
    if(NOT DEFINED ENV{HOMEBREW_LIBOMP_PREFIX})
        execute_process(
            COMMAND brew --prefix libomp
            OUTPUT_VARIABLE HOMEBREW_LIBOMP_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        set (HOMEBREW_LIBOMP_PREFIX $ENV{HOMEBREW_LIBOMP_PREFIX})
    endif()
    set(OpenMP_CXX_FLAGS
        "-Xpreprocessor -fopenmp -I${HOMEBREW_LIBOMP_PREFIX}/include")
    set(OpenMP_CXX_LIB_NAMES omp)
    find_library(OpenMP_omp_LIBRARY NAMES omp REQUIRED
                 HINTS ${HOMEBREW_LIBOMP_PREFIX}/lib)
    find_package(OpenMP REQUIRED)
else()
    find_package(OpenMP REQUIRED)
endif()
