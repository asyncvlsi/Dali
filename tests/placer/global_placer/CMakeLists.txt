cmake_minimum_required(VERSION 3.12)

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/placer/global_placer")
    return()
endif ()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else ()
    message(STATUS "GoogleTest main target not found; skipping tests/placer/global_placer")
    return()
endif ()

add_executable(global_upper_bound_refiner_test
               global_upper_bound_refiner_test.cc)
target_link_libraries(global_upper_bound_refiner_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME global_upper_bound_refiner_test
         COMMAND global_upper_bound_refiner_test)
set_tests_properties(global_upper_bound_refiner_test
                     PROPERTIES LABELS "unit;placer;global_placer")

add_executable(placement_capacity_model_test placement_capacity_model_test.cc)
target_link_libraries(placement_capacity_model_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME placement_capacity_model_test COMMAND placement_capacity_model_test)
set_tests_properties(placement_capacity_model_test
                     PROPERTIES LABELS "unit;placer;global_placer")
