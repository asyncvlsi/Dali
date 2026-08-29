find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/timing")
    return()
endif()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else()
    message(STATUS "GoogleTest main target not found; skipping tests/timing")
    return()
endif()

add_executable(timing_snapshot_test timing_snapshot_test.cc)
target_link_libraries(timing_snapshot_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME timing_snapshot_test COMMAND timing_snapshot_test)

add_executable(timing_decomposition_test timing_decomposition_test.cc)
target_link_libraries(timing_decomposition_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME timing_decomposition_test COMMAND timing_decomposition_test)

add_executable(timing_path_visualization_test timing_path_visualization_test.cc)
target_link_libraries(timing_path_visualization_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME timing_path_visualization_test COMMAND timing_path_visualization_test)

add_executable(delay_line_detour_test delay_line_detour_test.cc)
target_link_libraries(delay_line_detour_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME delay_line_detour_test COMMAND delay_line_detour_test)

add_executable(delay_line_feedback_test delay_line_feedback_test.cc)
target_link_libraries(delay_line_feedback_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME delay_line_feedback_test COMMAND delay_line_feedback_test)

add_executable(delay_line_gain_probe_test delay_line_gain_probe_test.cc)
target_link_libraries(delay_line_gain_probe_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME delay_line_gain_probe_test COMMAND delay_line_gain_probe_test)

add_executable(timing_driven_placement_controller_test
               timing_driven_placement_controller_test.cc)
target_link_libraries(timing_driven_placement_controller_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME timing_driven_placement_controller_test
         COMMAND timing_driven_placement_controller_test)

add_executable(timing_driven_placement_config_test
               timing_driven_placement_config_test.cc)
target_link_libraries(timing_driven_placement_config_test
                      PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME timing_driven_placement_config_test
         COMMAND timing_driven_placement_config_test)

add_executable(delay_line_extension_test delay_line_extension_test.cc)
target_link_libraries(delay_line_extension_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME delay_line_extension_test COMMAND delay_line_extension_test)
set_tests_properties(delay_line_extension_test
                     PROPERTIES LABELS "unit;timing")

add_executable(star_pi_net_coverage_test star_pi_net_coverage_test.cc)
target_link_libraries(star_pi_net_coverage_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME star_pi_net_coverage_test COMMAND star_pi_net_coverage_test)
set_tests_properties(star_pi_net_coverage_test PROPERTIES LABELS "unit;timing")

add_executable(delay_line_sizing_policy_test delay_line_sizing_policy_test.cc)
target_link_libraries(delay_line_sizing_policy_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME delay_line_sizing_policy_test COMMAND delay_line_sizing_policy_test)
set_tests_properties(delay_line_sizing_policy_test PROPERTIES LABELS "unit;timing")

add_executable(adaptive_delay_line_sizing_test adaptive_delay_line_sizing_test.cc)
target_link_libraries(adaptive_delay_line_sizing_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME adaptive_delay_line_sizing_test COMMAND adaptive_delay_line_sizing_test)
set_tests_properties(adaptive_delay_line_sizing_test PROPERTIES LABELS "unit;timing")

add_executable(topology_checkpoint_coordinator_test topology_checkpoint_coordinator_test.cc)
target_link_libraries(topology_checkpoint_coordinator_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME topology_checkpoint_coordinator_test COMMAND topology_checkpoint_coordinator_test)
set_tests_properties(topology_checkpoint_coordinator_test PROPERTIES LABELS "unit;timing")
