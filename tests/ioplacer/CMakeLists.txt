cmake_minimum_required(VERSION 3.9)

# prepare benchmark
add_test(NAME ioplacer_benchmark_preparation
        COMMAND tar -xvzf ispd19_test3.tar.gz
        WORKING_DIRECTORY ../tests/ioplacer)

# place all I/O pins on the same metal layer
add_executable(all_iopin_use_same_metal_layer
    all_iopin_use_same_metal_layer.cc helper.h helper.cc)
target_link_libraries(all_iopin_use_same_metal_layer
        PRIVATE dalilib)
add_test(NAME all_iopin_use_same_metal_layer
        COMMAND all_iopin_use_same_metal_layer
        WORKING_DIRECTORY ../tests/ioplacer)

# add and place I/O pins manually
add_executable(add_and_place_io_pins
    add_and_place_io_pins.cc helper.h helper.cc)
target_link_libraries(add_and_place_io_pins
    PRIVATE dalilib)
add_test(NAME add_and_place_io_pins
    COMMAND add_and_place_io_pins
    WORKING_DIRECTORY ../tests/ioplacer)
