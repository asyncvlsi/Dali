cmake_minimum_required(VERSION 3.9)

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

include_directories(${Boost_INCLUDE_DIR})

# compute cover area of a bunch of rectangles
add_executable(Boost_Tests_run
    misc_test.cc)
target_link_libraries(Boost_Tests_run
    PRIVATE dalilib
    ${Boost_LIBRARIES})
