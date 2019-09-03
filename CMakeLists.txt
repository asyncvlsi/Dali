cmake_minimum_required(VERSION 3.9)
project(HPCC)

include_directories(./src)

set(CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

file(GLOB SOURCES
    src/*.cc
    src/common/*.cc
    src/circuit/*.cc
    src/placer/*.cc
    src/placer/globalPlacer/*.cc
    src/placer/globalPlacer/GPSimPL/*.cc
    src/placer/detailedPlacer/*.cc
    src/placer/detailedPlacer/DPLinear/*.cc
    src/placer/legalizer/*.cc
    src/placer/legalizer/LGTetris/*.cc
    module/Eigen/*
    )

add_executable(HPCC ${SOURCES})
