cmake_minimum_required(VERSION 3.2)

# We only use C++ compiler for this project.
# By specifying the languages, project() will only check if there is a C++ compiler.
# Default values of LANGUAGES is C and C++, if not specified.
project(HPCC
    LANGUAGES CXX)

include_directories(./src)

set(CMAKE_CXX_STANDARD 11)
# The flags for g++, -g is needed here for debugging purposes, which can be removed
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")

# Set the output directory of static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Set the output directory of executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

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

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This placer library is not necessary, this library is created beacuse we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(placer
    STATIC
    ${SOURCES})

# Stand-alone placer, building from the placer library
add_executable(hpcc
    src/main.cc)
target_link_libraries(hpcc
    PRIVATE placer)

# Debugging exectuable, building from the placer library
add_executable(hpcc-debug
    EXCLUDE_FROM_ALL
    src/debug.cc)
target_link_libraries(hpcc-debug
    PRIVATE placer)

######################################
# Configure install destination directory
######################################
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

######################################
# Install header files
######################################
file(GLOB HEADER_COMMON
    src/common/*.h
    )

file(GLOB HEADER_CIRCUIT
    src/circuit/*.h
    )

file(GLOB HEADER_PLACER
    src/placer/*.h
    )

file(GLOB H_GLOBPLACER
    src/placer/globalPlacer/*.h
    )

file(GLOB H_GPSIMPL
    src/placer/globalPlacer/GPSimPL/*.h
    )

file(GLOB H_DPLACER
    src/placer/detailedPlacer/*.h
    )

file(GLOB H_DPLINEAR
    src/placer/detailedPlacer/DPLinear/*.h
    )

file(GLOB H_LEGAL
    src/placer/legalizer/*.h
    )

file(GLOB H_TETRIS
    src/placer/legalizer/LGTetris/*.h
    )

set(INLUDE_DESTIN include/hpcc)

install(FILES ${HEADER_COMMON}
    DESTINATION ${INLUDE_DESTIN}/common)
install(FILES ${HEADER_CIRCUIT}
    DESTINATION ${INLUDE_DESTIN}/circuit)
install(FILES ${HEADER_PLACER}
    DESTINATION ${INLUDE_DESTIN}/placer)
install(FILES ${H_GLOBPLACER}
    DESTINATION ${INLUDE_DESTIN}/placer/globalPlacer)
install(FILES ${H_GPSIMPL}
    DESTINATION ${INLUDE_DESTIN}/placer/globalPlacer/GPSimPL)
install(FILES ${H_DPLACER}
    DESTINATION ${INLUDE_DESTIN}/placer/detailedPlacer)
install(FILES ${H_DPLINEAR}
    DESTINATION ${INLUDE_DESTIN}/placer/detailedPlacer/DPLinear)
install(FILES ${H_LEGAL}
    DESTINATION ${INLUDE_DESTIN}/placer/legalizer)
install(FILES ${H_TETRIS}
    DESTINATION ${INLUDE_DESTIN}/placer/legalizer/LGTetris)

######################################
# Install binary
######################################
#install(TARGETS hpcc
#    DESTINATION bin)

######################################
# Install library
######################################
install(TARGETS placer
    DESTINATION lib)
