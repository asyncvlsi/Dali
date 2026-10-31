cmake_minimum_required(VERSION 3.12)

add_executable(consensus_core_test consensus_core_test.cc)
target_link_libraries(consensus_core_test PRIVATE dalilib)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(consensus_core_test PRIVATE -ffp-contract=off)
endif()
add_test(NAME consensus_core_test COMMAND consensus_core_test)
set_tests_properties(consensus_core_test PROPERTIES LABELS "unit;placer;consensus")
add_test(NAME consensus_cli_test COMMAND ${CMAKE_COMMAND}
  -DDALI=$<TARGET_FILE:dali>
  -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/testdata/flow
  -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-output
  -P ${CMAKE_CURRENT_SOURCE_DIR}/consensus_cli_test.cmake)
set_tests_properties(consensus_cli_test PROPERTIES
  LABELS "integration;placer;consensus" TIMEOUT 60)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "GoogleTest not found; skipping ConsensusPlacer integration tests")
  return()
endif()
if(TARGET GTest::gtest_main)
  set(CONSENSUS_GTEST GTest::gtest_main)
else()
  set(CONSENSUS_GTEST GTest::Main)
endif()
add_executable(consensus_placer_test consensus_placer_test.cc)
target_link_libraries(consensus_placer_test PRIVATE dalilib ${CONSENSUS_GTEST})
target_compile_definitions(consensus_placer_test PRIVATE
  CONSENSUS_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
add_test(NAME consensus_placer_test COMMAND consensus_placer_test)
set_tests_properties(consensus_placer_test PROPERTIES LABELS "unit;placer;consensus")

add_test(NAME consensus_multi_region_cli_test COMMAND ${CMAKE_COMMAND}
  -DDALI=$<TARGET_FILE:dali>
  -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/testdata/multiflow
  -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/multi-cli-output
  -P ${CMAKE_CURRENT_SOURCE_DIR}/multi_region_cli_test.cmake)
set_tests_properties(consensus_multi_region_cli_test PROPERTIES
  LABELS "integration;placer;consensus" TIMEOUT 60)
