cmake_minimum_required(VERSION 3.10)

project(
        Dali
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        module/eigen-git-mirror
)

# add sub-cmake files to the search path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

############################################################################
# Detect if $ACT_HOME exits
############################################################################
message(STATUS "Detecting environment variable ACT_HOME...")
if (DEFINED ENV{ACT_HOME})
    message(STATUS "Environment variable ACT_HOME detected: " $ENV{ACT_HOME})
else ()
    message(FATAL_ERROR "Environment variable ACT_HOME not found")
endif ()
include_directories($ENV{ACT_HOME}/include)
link_directories($ENV{ACT_HOME}/lib)
find_library(ACT_LIBRARY NAMES act PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libact.a: " ${ACT_LIBRARY})
find_library(ACTVLSILIB_LIBRARY NAMES vlsilib PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libvlsilib.a: " ${ACTVLSILIB_LIBRARY})
find_library(PHYDB_LIBRARY NAMES phydb PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libphydb.a: " ${PHYDB_LIBRARY})
find_library(LEF_LIBRARY NAMES lef PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found liblef.a: " ${LEF_LIBRARY})
find_library(DEF_LIBRARY NAMES def PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libdef.a: " ${DEF_LIBRARY})

############################################################################
# Detect if galois_eda library exists
############################################################################
include(cmake/FindGaloisEDA.cmake)

############################################################################
# Detect if OpenMP library exists
############################################################################
include(cmake/FindOpenMPPackage.cmake)

############################################################################
# Detect if Boost library exists
############################################################################
#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.71.0 COMPONENTS log_setup log REQUIRED)
message(STATUS "Boost library path: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost include path: ${Boost_INCLUDE_DIR}")
message(STATUS "Boost libs: ${Boost_LIBRARIES}")
include_directories(${Boost_INCLUDE_DIRS})

############################################################################
# Detect if CPLEX library exists
############################################################################
include(cmake/FindCPLEX.cmake)

############################################################################
# Set Git Commit
############################################################################
include(cmake/FindGitCommitVersion.cmake)

############################################################################
# for building a standalone static executable
############################################################################
set(BUILD_STATIC_EXECUTABLES FALSE)
if (BUILD_STATIC_EXECUTABLES)
    set(CMAKE_EXE_LINKER_FLAGS -static)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
    set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
    set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
    set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
    set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
    # Maybe this works as well, haven't tried yet.
    # set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
else (BUILD_STATIC_EXECUTABLES)
    # Set RPATH to use for installed targets; append linker search path
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LOFAR_LIBDIR}")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif (BUILD_STATIC_EXECUTABLES)

# Set C++ compilation flags and options
set(CMAKE_CXX_FLAGS_DEBUG "-g -O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Set a default build type if none was specified
set(default_build_type "RELEASE")
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}")
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "DEBUG" "RELEASE")
endif ()

if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "DEBUG build type")
else (CMAKE_BUILD_TYPE MATCHES RELEASE)
    message(STATUS "RELEASE build type")
endif ()

add_compile_options(-fopenmp)
add_compile_options(-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Werror=return-type -pedantic)


# Set the output directory of static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Set the output directory of executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# All used circuit and placer source code
file(
        GLOB_RECURSE SOURCES
        dali/dali.cc
        dali/circuit/*.cc
        dali/common/*.cc
        dali/placer/*.cc
        dali/timing/*.cc
)

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This dalilib library is not necessary, this library is created because we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(
        dalilib
        STATIC
        ${SOURCES}
        dali/common/named_instance_collection.h
)

if (DEFINED NEED_LIBCXXFS)
    set(LIBCXXFS_LIB "stdc++fs")
else ()
    set(LIBCXXFX_LIB "")
endif ()

target_link_libraries(
        dalilib
        ${ACT_LIBRARY} ${ACTVLSILIB_LIBRARY} ${PHYDB_LIBRARY}
        ${Galois_LIBRARIES}
        ${LEF_LIBRARY} ${DEF_LIBRARY}
        ${CPLEX_LIBRARIES} m pthread dl
        ${Boost_LIBRARIES}
        OpenMP::OpenMP_CXX
        ${LIBCXXFS_LIB}
)

# Stand-alone placer, building from the dalilib library
add_executable(
        dali
        dali/application/dali_standalone.cc
)
target_link_libraries(
        dali
        PRIVATE dalilib
)

# Debugging executable, building from the dalilib library
add_executable(
        dali-debug
        dali/application/debug.cc
)
target_link_libraries(
        dali-debug
        PRIVATE dalilib
)

# Debugging executable, building from the dalilib library
add_executable(
        wellflow
        dali/application/wellflow.cc
)
target_link_libraries(
        wellflow
        PRIVATE dalilib
)

# Debugging executable, building from the dalilib library
add_executable(
        adaptec-debug
        dali/application/debug-adaptec.cc
)
target_link_libraries(
        adaptec-debug
        PRIVATE dalilib
)

# Stand-alone hpwl calculator, building from the dalilib library
add_executable(
        hpwl
        dali/application/hpwl.cc
)
target_link_libraries(
        hpwl
        PRIVATE dalilib
)

# Stand-alone LEFDEF to Bookshelf format tool, building from the dalilib library
add_executable(
        lefdef2bookshelf
        dali/application/lefdef2bookshelf.cc
)
target_link_libraries(
        lefdef2bookshelf
        PRIVATE dalilib
)

# Stand-alone Bookshelf to LEFDEF format tool, building from the dalilib library
add_executable(
        bookshelf2def
        dali/application/bookshelf2def.cc
)
target_link_libraries(
        bookshelf2def
        PRIVATE dalilib
)

# Stand-alone custom cell design to standard cell design tool
add_executable(
        create_circuit
        dali/application/create_circuit.cc
)
target_link_libraries(
        create_circuit
        PRIVATE dalilib
)

# Mixed-cell-height legalization for standard cells
add_executable(
        mhlg
        dali/application/mhlg.cc
)
target_link_libraries(
        mhlg
        PRIVATE dalilib
)

# Unittests
enable_testing()
add_subdirectory(tests/boost_test)
add_subdirectory(tests/ioplacer)

############################################################################
# Configure install destination directory
############################################################################
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(STATUS "Changing default installation directory to $ACT_HOME")
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
message(STATUS "Current installation directory: " ${CMAKE_INSTALL_PREFIX})

############################################################################
# Install header files
############################################################################
set(INCLUDE_DESTIN include)
install(
        DIRECTORY dali/
        DESTINATION ${INCLUDE_DESTIN}/dali
        FILES_MATCHING
        PATTERN "*.h"
)
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/module/eigen-git-mirror/Eigen/
        DESTINATION ${INCLUDE_DESTIN}/Eigen
)

############################################################################
# Install binary
############################################################################
install(
        TARGETS dali
        DESTINATION bin
)

############################################################################
# Install library
############################################################################
install(
        TARGETS dalilib
        DESTINATION lib
)
