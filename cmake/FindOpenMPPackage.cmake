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
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -I${HOMEBREW_LIBOMP_PREFIX}/include")
    set(OpenMP_CXX_LIB_NAMES omp)
    set(OpenMP_omp_LIBRARY ${HOMEBREW_LIBOMP_PREFIX}/lib/libomp.dylib ${HOMEBREW_LIBOMP_PREFIX}/lib/libomp.a)
    find_package(OpenMP REQUIRED)
    add_compile_options(-Xpreprocessor)
else()
    find_package(OpenMP REQUIRED)
endif()
