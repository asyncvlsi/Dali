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

add_executable(stage_band_test stage_band_test.cc)
target_link_libraries(stage_band_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME stage_band_test COMMAND stage_band_test)
set_tests_properties(stage_band_test
                     PROPERTIES LABELS "unit;placer;global_placer")

add_executable(placement_checkpoint_test placement_checkpoint_test.cc)
target_link_libraries(placement_checkpoint_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME placement_checkpoint_test COMMAND placement_checkpoint_test)
set_tests_properties(placement_checkpoint_test
                     PROPERTIES LABELS "unit;placer;global_placer")

add_executable(checkpoint_boundary_test checkpoint_boundary_test.cc)
target_link_libraries(checkpoint_boundary_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME checkpoint_boundary_test COMMAND checkpoint_boundary_test)
set_tests_properties(checkpoint_boundary_test
                     PROPERTIES LABELS "unit;placer;global_placer")

add_executable(optimizer_anchor_state_test optimizer_anchor_state_test.cc)
target_link_libraries(optimizer_anchor_state_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME optimizer_anchor_state_test COMMAND optimizer_anchor_state_test)
set_tests_properties(optimizer_anchor_state_test
                     PROPERTIES LABELS "unit;placer;global_placer")
