cmake_minimum_required(VERSION 3.12)

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/placer/standard_cell_legalizer")
    return()
endif ()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else ()
    message(STATUS "GoogleTest main target not found; skipping tests/placer/standard_cell_legalizer")
    return()
endif ()

add_executable(
    standard_cell_placement_model_test
    standard_cell_placement_model_test.cc
)
target_link_libraries(standard_cell_placement_model_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(
    NAME standard_cell_placement_model_test
    COMMAND standard_cell_placement_model_test
)
set_tests_properties(standard_cell_placement_model_test
                     PROPERTIES LABELS "unit;placer;standard_cell_legalizer")
