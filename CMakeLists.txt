cmake_minimum_required(VERSION 3.2)

# We only use C++ compiler for this project, by specifying the languages, project() will only check if there is a C++ compiler
# Default values of LANGUAGES is C and C++, if not specified.
project(HPCC
    LANGUAGES CXX)

include_directories(./src)

set(CMAKE_CXX_STANDARD 11)
# The flags for g++, -g is needed here for debugging purposes, which can be removed
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# All used circuit and placer source code
file(GLOB SOURCES
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

# Stand-alone placer
add_executable(hpcc src/main.cc ${SOURCES})

# Debugging exectuable
add_executable(hpcc-debug EXCLUDE_FROM_ALL src/debug.cc ${SOURCES})
