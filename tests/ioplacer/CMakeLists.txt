cmake_minimum_required(VERSION 3.9)

add_test(NAME ioplacer_benchmark_preparation
        COMMAND tar -xvzf ispd19_test3.tar.gz
        WORKING_DIRECTORY ../tests/ioplacer)

add_executable(metal_layer
        metal_layer.cc helper.h helper.cc)
target_link_libraries(metal_layer
        PRIVATE dalilib)
add_test(NAME metal_layer
        COMMAND metal_layer
        WORKING_DIRECTORY ../tests/ioplacer)
