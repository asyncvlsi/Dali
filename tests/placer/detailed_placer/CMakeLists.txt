cmake_minimum_required(VERSION 3.12)

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/placer/detailed_placer")
    return()
endif ()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else ()
    message(STATUS "GoogleTest main target not found; skipping tests/placer/detailed_placer")
    return()
endif ()

add_executable(detailed_placer_local_reorder_test local_reorder_test.cc)
target_link_libraries(detailed_placer_local_reorder_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(
    NAME detailed_placer_local_reorder_test
    COMMAND detailed_placer_local_reorder_test
)
set_tests_properties(detailed_placer_local_reorder_test
                     PROPERTIES LABELS "unit;placer;detailed_placer")
