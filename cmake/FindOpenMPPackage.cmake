############################################################################
# Find OpenMP package
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
    # AppleClang needs -fopenmp handed to the preprocessor. Keep the two tokens
    # adjacent here: passing -Xpreprocessor separately relies on it landing
    # immediately before -fopenmp, and it silently consumes whatever flag
    # follows it instead.
    set(OpenMP_CXX_FLAGS
        "-Xpreprocessor -fopenmp -I${HOMEBREW_LIBOMP_PREFIX}/include")
    set(OpenMP_CXX_LIB_NAMES omp)
    set(OpenMP_omp_LIBRARY ${HOMEBREW_LIBOMP_PREFIX}/lib/libomp.dylib)
    find_package(OpenMP REQUIRED)
else()
    find_package(OpenMP REQUIRED)
endif()