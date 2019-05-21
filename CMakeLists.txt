cmake_minimum_required(VERSION 3.13)
project(HPCC)

include_directories(./src)

set(CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

file(GLOB SOURCES
        src/*.hpp
        src/*.cpp
        src/DLA/*.hpp
        src/DLA/*.cpp
        )

add_executable(HPCC ${SOURCES})
