cmake_minimum_required(VERSION 3.12)

# Prepare benchmark
add_test(
    NAME io_placer_benchmark_preparation
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_TAR=${CMAKE_CURRENT_SOURCE_DIR}/ispd19_test3.tar.gz
        -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/prepare_benchmark.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(io_placer_benchmark_preparation
                     PROPERTIES LABELS "integration;io_placer")

# Place all I/O pins on the same metal layer
add_executable(all_iopin_use_same_metal_layer all_iopin_use_same_metal_layer.cc helper.h helper.cc)
target_link_libraries(all_iopin_use_same_metal_layer PRIVATE dalilib)
add_test(
    NAME all_iopin_use_same_metal_layer
    COMMAND all_iopin_use_same_metal_layer
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(all_iopin_use_same_metal_layer
                     PROPERTIES LABELS "integration;io_placer")

# Add and place I/O pins manually
add_executable(add_and_place_io_pins add_and_place_io_pins.cc helper.h helper.cc)
target_link_libraries(add_and_place_io_pins PRIVATE dalilib)
add_test(
    NAME add_and_place_io_pins
    COMMAND add_and_place_io_pins
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(add_and_place_io_pins
                     PROPERTIES LABELS "integration;io_placer")

# Place I/O pins through the argv-style Dali::IoPinPlacement command
add_executable(io_pin_placement_argv_command io_pin_placement_argv_command.cc helper.h helper.cc)
target_link_libraries(io_pin_placement_argv_command PRIVATE dalilib)
add_test(
    NAME io_pin_placement_argv_command
    COMMAND io_pin_placement_argv_command
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(io_pin_placement_argv_command
                     PROPERTIES LABELS "integration;io_placer")

# Manually place an I/O pin at an interior location
add_executable(manual_place_io_pin manual_place_io_pin.cc helper.h helper.cc)
target_link_libraries(manual_place_io_pin PRIVATE dalilib)
add_test(
    NAME manual_place_io_pin
    COMMAND manual_place_io_pin
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(manual_place_io_pin
                     PROPERTIES LABELS "integration;io_placer")

# Constrain an I/O pin to a specific edge
add_executable(constrain_io_pin_to_edge constrain_io_pin_to_edge.cc helper.h helper.cc)
target_link_libraries(constrain_io_pin_to_edge PRIVATE dalilib)
add_test(
    NAME constrain_io_pin_to_edge
    COMMAND constrain_io_pin_to_edge
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(constrain_io_pin_to_edge
                     PROPERTIES LABELS "integration;io_placer")
