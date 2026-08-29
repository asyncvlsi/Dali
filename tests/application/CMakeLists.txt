cmake_minimum_required(VERSION 3.12)

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/application")
    return()
endif ()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else ()
    message(STATUS "GoogleTest main target not found; skipping tests/application")
    return()
endif ()

function(add_dali_application_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} PRIVATE dalilib ${DALI_GTEST_MAIN})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit;application")
endfunction()

add_dali_application_test(application_dali_command_line_test
                          dali_command_line_test.cc)
add_dali_application_test(application_dali_command_processor_test
                          dali_command_processor_test.cc)
add_dali_application_test(application_dali_config_test dali_config_test.cc)
add_dali_application_test(application_dali_placement_flow_test
                          dali_placement_flow_test.cc)

add_executable(application_rebind_phydb_test rebind_phydb_test.cc)
target_link_libraries(application_rebind_phydb_test PRIVATE dalilib ${DALI_GTEST_MAIN})
add_test(NAME application_rebind_phydb_test COMMAND application_rebind_phydb_test)
set_tests_properties(application_rebind_phydb_test PROPERTIES LABELS "unit;application")
