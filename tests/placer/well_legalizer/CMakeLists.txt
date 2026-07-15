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

add_executable(gridded_row_test gridded_row_test.cc)
target_link_libraries(gridded_row_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME gridded_row_test COMMAND gridded_row_test)
set_tests_properties(gridded_row_test
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

add_executable(gridded_row_assignment_transaction_test
               gridded_row_assignment_transaction_test.cc)
target_link_libraries(gridded_row_assignment_transaction_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME gridded_row_assignment_transaction_test
         COMMAND gridded_row_assignment_transaction_test)
set_tests_properties(gridded_row_assignment_transaction_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(ortools_fixed_row_displacement_optimizer_test
               ortools_fixed_row_displacement_optimizer_test.cc)
target_link_libraries(ortools_fixed_row_displacement_optimizer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME ortools_fixed_row_displacement_optimizer_test
         COMMAND ortools_fixed_row_displacement_optimizer_test)
set_tests_properties(ortools_fixed_row_displacement_optimizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(ortools_gridded_row_optimizer_test
               ortools_gridded_row_optimizer_test.cc)
target_link_libraries(ortools_gridded_row_optimizer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME ortools_gridded_row_optimizer_test
         COMMAND ortools_gridded_row_optimizer_test)
set_tests_properties(ortools_gridded_row_optimizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(exact_gridded_legalization_model_test
               exact_gridded_legalization_model_test.cc)
target_link_libraries(exact_gridded_legalization_model_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME exact_gridded_legalization_model_test
         COMMAND exact_gridded_legalization_model_test)
set_tests_properties(exact_gridded_legalization_model_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(exact_gridded_legalization_model_builder_test
               exact_gridded_legalization_model_builder_test.cc)
target_link_libraries(exact_gridded_legalization_model_builder_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME exact_gridded_legalization_model_builder_test
         COMMAND exact_gridded_legalization_model_builder_test)
set_tests_properties(exact_gridded_legalization_model_builder_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(exact_gridded_whole_design_model_builder_test
               exact_gridded_whole_design_model_builder_test.cc)
target_link_libraries(exact_gridded_whole_design_model_builder_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME exact_gridded_whole_design_model_builder_test
         COMMAND exact_gridded_whole_design_model_builder_test)
set_tests_properties(exact_gridded_whole_design_model_builder_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(exact_gridded_stripe_model_builder_test
               exact_gridded_stripe_model_builder_test.cc)
target_link_libraries(exact_gridded_stripe_model_builder_test
                      PRIVATE dalilib GTest::gtest_main)
add_test(NAME exact_gridded_stripe_model_builder_test
         COMMAND exact_gridded_stripe_model_builder_test)
set_tests_properties(exact_gridded_stripe_model_builder_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(exact_gridded_boundary_model_builder_test
               exact_gridded_boundary_model_builder_test.cc)
target_link_libraries(exact_gridded_boundary_model_builder_test
                      PRIVATE dalilib GTest::gtest_main)
add_test(NAME exact_gridded_boundary_model_builder_test
         COMMAND exact_gridded_boundary_model_builder_test)
set_tests_properties(exact_gridded_boundary_model_builder_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(exact_gridded_legalization_window_analyzer_test
               exact_gridded_legalization_window_analyzer_test.cc)
target_link_libraries(exact_gridded_legalization_window_analyzer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME exact_gridded_legalization_window_analyzer_test
         COMMAND exact_gridded_legalization_window_analyzer_test)
set_tests_properties(exact_gridded_legalization_window_analyzer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(ortools_exact_gridded_legalizer_test
               ortools_exact_gridded_legalizer_test.cc)
target_link_libraries(ortools_exact_gridded_legalizer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME ortools_exact_gridded_legalizer_test
         COMMAND ortools_exact_gridded_legalizer_test)
set_tests_properties(ortools_exact_gridded_legalizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(ortools_compact_gridded_legalizer_test
               ortools_compact_gridded_legalizer_test.cc)
target_link_libraries(ortools_compact_gridded_legalizer_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME ortools_compact_gridded_legalizer_test
         COMMAND ortools_compact_gridded_legalizer_test)
set_tests_properties(ortools_compact_gridded_legalizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")

add_executable(ortools_gridded_stripe_optimizer_test
               ortools_gridded_stripe_optimizer_test.cc)
target_link_libraries(ortools_gridded_stripe_optimizer_test
                      PRIVATE dalilib GTest::gtest_main)
add_test(NAME ortools_gridded_stripe_optimizer_test
         COMMAND ortools_gridded_stripe_optimizer_test)
set_tests_properties(ortools_gridded_stripe_optimizer_test
                     PROPERTIES LABELS "unit;placer;well_legalizer")
