cmake_minimum_required(VERSION 3.9)

add_test(NAME ioplacer_benchmark_preparation
        COMMAND tar -xvzf ispd19_test3.tar.gz
        WORKING_DIRECTORY ../tests/ioplacer)

add_executable(all_iopin_use_same_metal_layer
    all_iopin_use_same_metal_layer.cc helper.h helper.cc)
target_link_libraries(all_iopin_use_same_metal_layer
        PRIVATE dalilib)
add_test(NAME all_iopin_use_same_metal_layer
        COMMAND all_iopin_use_same_metal_layer
        WORKING_DIRECTORY ../tests/ioplacer)
