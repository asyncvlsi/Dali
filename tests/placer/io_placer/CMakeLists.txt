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
                     PROPERTIES
                         LABELS "integration;io_placer"
                         FIXTURES_SETUP io_placer_benchmark)

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

# Area-array (interior) I/O placement
add_executable(area_array_place_io_pin area_array_place_io_pin.cc helper.h helper.cc)
target_link_libraries(area_array_place_io_pin PRIVATE dalilib)
add_test(
    NAME area_array_place_io_pin
    COMMAND area_array_place_io_pin
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(area_array_place_io_pin
                     PROPERTIES LABELS "integration;io_placer")

# Group I/O pins as a contiguous run along an edge
add_executable(group_io_pins group_io_pins.cc helper.h helper.cc)
target_link_libraries(group_io_pins PRIVATE dalilib)
add_test(
    NAME group_io_pins
    COMMAND group_io_pins
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(group_io_pins PROPERTIES LABELS "integration;io_placer")

# Mirror an I/O pin across a die center axis
add_executable(mirror_io_pin mirror_io_pin.cc helper.h helper.cc)
target_link_libraries(mirror_io_pin PRIVATE dalilib)
add_test(
    NAME mirror_io_pin
    COMMAND mirror_io_pin
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(mirror_io_pin PROPERTIES LABELS "integration;io_placer")

# Preserve pre-placement I/O commands when a `.dali` recipe later runs the
# ordinary placement pipeline.
add_executable(io_command_file_flow io_command_file_flow.cc helper.h helper.cc)
target_link_libraries(io_command_file_flow PRIVATE dalilib)
add_test(
    NAME io_command_file_flow
    COMMAND io_command_file_flow
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(io_command_file_flow
                     PROPERTIES LABELS "integration;io_placer")

# Load LEF/DEF from a recipe, run Dali, and export through `write-def`.
add_executable(script_owned_design_flow script_owned_design_flow.cc)
target_link_libraries(script_owned_design_flow PRIVATE dalilib)
add_test(
    NAME script_owned_design_flow
    COMMAND script_owned_design_flow
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(script_owned_design_flow
                     PROPERTIES LABELS "integration;io_placer;application")

# Inspect, move, validate, release, and automatically replace I/O pins through
# the command processor.
add_executable(io_signoff_commands io_signoff_commands.cc helper.h helper.cc)
target_link_libraries(io_signoff_commands PRIVATE dalilib)
add_test(
    NAME io_signoff_commands
    COMMAND io_signoff_commands
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(io_signoff_commands
                     PROPERTIES LABELS "integration;io_placer")

set(io_placer_tests
    all_iopin_use_same_metal_layer
    add_and_place_io_pins
    io_pin_placement_argv_command
    manual_place_io_pin
    constrain_io_pin_to_edge
    area_array_place_io_pin
    group_io_pins
    mirror_io_pin
    io_command_file_flow
    script_owned_design_flow
    io_signoff_commands
)
set_tests_properties(${io_placer_tests}
                     PROPERTIES FIXTURES_REQUIRED io_placer_benchmark)
