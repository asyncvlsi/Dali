cmake_minimum_required(VERSION 3.9)

# We only use C++ compiler for this project.
# By specifying the languages, project() will only check if there is a C++ compiler.
# Default values of LANGUAGES is C and C++, if not specified.
project(HPCC
    LANGUAGES CXX)

include_directories(./src)

#set(CMAKE_CXX_COMPILER /usr/local/bin/g++-8)

set(CMAKE_CXX_STANDARD 11)
# The flags for g++, -g is needed here for debugging purposes, which can be removed
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")

# Set the output directory of static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Set the output directory of executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

#add_subdirectory(module/eigen-git-mirror)
add_subdirectory(module/OpenDB)

include_directories(
    module/OpenDB/include/lef56
    module/OpenDB/include/opendb
    module/OpenDB/src/db
    module/OpenDB/src/defin
    module/OpenDB/src/defout
    module/OpenDB/src/lef56
    module/OpenDB/src/lefin
    module/OpenDB/src/lefout
    module/OpenDB/src/swig
    module/OpenDB/src/tm
    module/OpenDB/src/zlib
    module/OpenDB/src/zutil
)

# All used circuit and placer source code
file(GLOB SOURCES
        src/opendb.cc
        src/common/*.cc
        src/circuit/*.cc
        src/placer/*.cc
        src/placer/globalPlacer/*.cc
        src/placer/globalPlacer/GPSimPL/*.cc
        src/placer/detailedPlacer/*.cc
        src/placer/detailedPlacer/DPLinear/*.cc
        src/placer/detailedPlacer/MDPlacer/*.cc
        src/placer/legalizer/*.cc
        src/placer/legalizer/LGTetris/*.cc
        src/placer/wellLegalizer/*.cc
        )

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This placer library is not necessary, this library is created because we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(placer
    STATIC
    ${SOURCES})

# Stand-alone placer, building from the placer library
add_executable(hpcc
    src/hpcc.cc)
target_link_libraries(hpcc
    PRIVATE placer)

# Debugging exectuable, building from the placer library
add_executable(hpcc-debug
    EXCLUDE_FROM_ALL
    src/debug.cc)
add_dependencies( hpcc-debug opendb )

target_link_libraries(hpcc-debug
    PRIVATE placer
    opendb)

# Stand-alone hpwl calculator, building from the placer library
add_executable(hpwl
    EXCLUDE_FROM_ALL
    src/hpwl.cc)
target_link_libraries(hpwl
    PRIVATE placer)

######################################
# Configure install destination directory
######################################
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "installation path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

######################################
# Detect if $ACT_HOME exits, change the installation directory to ACT_HOME
######################################
message(STATUS "Detecting environment variable ACT_HOME...")
if(DEFINED ENV{ACT_HOME})
  message(STATUS "Environment variable ACT_HOME detected: " $ENV{ACT_HOME})
  if(FLASE)
    message(STATUS "Changing Installation directory to ACT_HOME")
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
    message(STATUS "Current installation directory: " ${CMAKE_INSTALL_PREFIX})
  endif()
else()
  message(STATUS "Environment variable ACT_HOME not found")
endif()

######################################
# Install header files
######################################
set(INLUDE_DESTIN include)
install(DIRECTORY src/ DESTINATION ${INLUDE_DESTIN}/hpcc COMPONENT Devel FILES_MATCHING PATTERN "*.h")

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
