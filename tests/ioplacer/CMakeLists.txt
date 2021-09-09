cmake_minimum_required(VERSION 3.9)

add_executable(metal
        metal.cc)
target_link_libraries(metal
        PRIVATE dalilib)
