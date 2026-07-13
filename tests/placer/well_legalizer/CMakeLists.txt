cmake_minimum_required(VERSION 3.12)

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/placer/well_legalizer")
    return()
endif ()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else ()
    message(STATUS "GoogleTest main target not found; skipping tests/placer/well_legalizer")
    return()
endif ()

add_executable(well_row_completer_test well_row_completer_test.cc)
target_link_libraries(well_row_completer_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME well_row_completer_test COMMAND well_row_completer_test)
set_tests_properties(well_row_completer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(well_geometry_test well_geometry_test.cc)
target_link_libraries(well_geometry_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME well_geometry_test COMMAND well_geometry_test)
set_tests_properties(well_geometry_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(gridded_capacity_estimator_test
               gridded_capacity_estimator_test.cc)
target_link_libraries(gridded_capacity_estimator_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME gridded_capacity_estimator_test
         COMMAND gridded_capacity_estimator_test)
set_tests_properties(gridded_capacity_estimator_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(adaptive_stripe_boundary_planner_test
               adaptive_stripe_boundary_planner_test.cc)
target_link_libraries(adaptive_stripe_boundary_planner_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME adaptive_stripe_boundary_planner_test
         COMMAND adaptive_stripe_boundary_planner_test)
set_tests_properties(adaptive_stripe_boundary_planner_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(packed_stripe_boundary_planner_test
               packed_stripe_boundary_planner_test.cc)
target_link_libraries(packed_stripe_boundary_planner_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME packed_stripe_boundary_planner_test
         COMMAND packed_stripe_boundary_planner_test)
set_tests_properties(packed_stripe_boundary_planner_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(stripe_boundary_coordinate_optimizer_test
               stripe_boundary_coordinate_optimizer_test.cc)
target_link_libraries(stripe_boundary_coordinate_optimizer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME stripe_boundary_coordinate_optimizer_test
         COMMAND stripe_boundary_coordinate_optimizer_test)
set_tests_properties(stripe_boundary_coordinate_optimizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(gridded_stripe_balancer_test gridded_stripe_balancer_test.cc)
target_link_libraries(gridded_stripe_balancer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME gridded_stripe_balancer_test COMMAND gridded_stripe_balancer_test)
set_tests_properties(gridded_stripe_balancer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(gridded_row_location_optimizer_test
               gridded_row_location_optimizer_test.cc)
target_link_libraries(gridded_row_location_optimizer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME gridded_row_location_optimizer_test
         COMMAND gridded_row_location_optimizer_test)
set_tests_properties(gridded_row_location_optimizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(gridded_detailed_placer_test gridded_detailed_placer_test.cc)
target_link_libraries(gridded_detailed_placer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME gridded_detailed_placer_test COMMAND gridded_detailed_placer_test)
set_tests_properties(gridded_detailed_placer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")
