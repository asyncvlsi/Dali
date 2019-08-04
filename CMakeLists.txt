cmake_minimum_required(VERSION 3.9)
project(HPCC)

include_directories(./src)

set(CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

file(GLOB SOURCES
        src/common/*.h
        src/common/*.cc
        src/circuit/*.h
        src/circuit/*.cc
        src/placer/*.h
        src/placer/*.cc
        src/placer/globalPlacer/*.h
        src/placer/globalPlacer/*.cc
        src/placer/legalizer/*.h
        src/placer/legalizer/*.cc
        src/*.h
        src/*.cc
        )

add_executable(HPCC ${SOURCES})
