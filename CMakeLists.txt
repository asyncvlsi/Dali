cmake_minimum_required(VERSION 3.12)

project(
    Dali
    LANGUAGES CXX
)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Include Directories& Module Path
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    module/eigen-git-mirror
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Dependency Detection : ACT_HOME
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
message(STATUS "Detecting environment variable ACT_HOME...")
if (DEFINED ENV{ACT_HOME})
    message(STATUS "Environment variable ACT_HOME detected: $ENV{ACT_HOME}")
else ()
    message(FATAL_ERROR "Environment variable ACT_HOME not found")
endif ()

include_directories($ENV{ACT_HOME}/include)
link_directories($ENV{ACT_HOME}/lib)

find_library(ACT_LIBRARY NAMES act PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libact.a: ${ACT_LIBRARY}")

find_library(ACTVLSILIB_LIBRARY NAMES vlsilib PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libvlsilib.a: ${ACTVLSILIB_LIBRARY}")

find_library(PHYDB_LIBRARY NAMES phydb PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libphydb.a: ${PHYDB_LIBRARY}")

find_library(LEF_LIBRARY NAMES lef PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found liblef.a: ${LEF_LIBRARY}")

find_library(DEF_LIBRARY NAMES def PATHS $ENV{ACT_HOME}/lib REQUIRED)
message(STATUS "Found libdef.a: ${DEF_LIBRARY}")

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Dependencies : Galois, OpenMP, CPLEX, Git Commit
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Detect if galois_eda library exists
include(cmake/FindGaloisEDA.cmake)

#Detect if OpenMP library exists
include(cmake/FindOpenMPPackage.cmake)

#Detect if CPLEX library exists
include(cmake/FindCPLEX.cmake)

#Set Git Commit version
include(cmake/FindGitCommitVersion.cmake)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Build Types and Options
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
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
else ()
#Set RPATH to use for installed targets; append linker search path
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif ()

#Set C++ compilation flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

#Set default build type to RELEASE if none specified
set(default_build_type "RELEASE")
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "DEBUG" "RELEASE")
endif ()

if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "DEBUG build type")
else ()
    message(STATUS "RELEASE build type")
endif ()

add_compile_options(-fopenmp)
add_compile_options(-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Werror=return-type -pedantic)

set(DALI_GUI "AUTO" CACHE STRING "Qt live placement GUI mode: AUTO, ON, or OFF")
set_property(CACHE DALI_GUI PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${DALI_GUI}" DALI_GUI_MODE)

foreach(qt_prefix
        /opt/homebrew/opt/qt
        /opt/homebrew/opt/qt@6
        /usr/local/opt/qt
        /usr/local/opt/qt@6)
    if (EXISTS "${qt_prefix}/lib/cmake/Qt6")
        list(APPEND CMAKE_PREFIX_PATH "${qt_prefix}")
    endif ()
endforeach()

find_program(BREW_EXECUTABLE brew)
if (BREW_EXECUTABLE)
    foreach(qt_formula qt qt@6)
        execute_process(
            COMMAND ${BREW_EXECUTABLE} --prefix ${qt_formula}
            RESULT_VARIABLE brew_qt_result
            OUTPUT_VARIABLE brew_qt_prefix
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (brew_qt_result EQUAL 0 AND EXISTS "${brew_qt_prefix}/lib/cmake/Qt6")
            list(APPEND CMAKE_PREFIX_PATH "${brew_qt_prefix}")
        endif ()
    endforeach()
endif ()

if (DALI_GUI_MODE STREQUAL "OFF")
    set(DALI_HAS_QT_GUI FALSE)
else ()
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if (Qt6Widgets_FOUND)
        message(STATUS "Qt6 Widgets found: enabling Dali GUI debug support")
        set(DALI_HAS_QT_GUI TRUE)
    else ()
        set(DALI_HAS_QT_GUI FALSE)
        if (DALI_GUI_MODE STREQUAL "ON")
            message(FATAL_ERROR "DALI_GUI=ON but Qt6 Widgets was not found")
        else ()
            message(STATUS "Qt6 Widgets not found: building Dali without GUI debug support")
        endif ()
    endif ()
endif ()

#Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Library Target
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Glob all used circuit and placer source code
file(
    GLOB_RECURSE SOURCES
    dali/dali.cc
    dali/application/dali_command_line.cc
    dali/circuit/*.cc
    dali/common/*.cc
    dali/placer/*.cc
    dali/timing/*.cc
)

# Create the dalilib static library
add_library(
    dalilib
    STATIC
    ${SOURCES}
    dali/common/named_instance_registry.h
    dali/common/placement_snapshot_writer.cc
    dali/common/placement_snapshot_writer.h
)

if (DEFINED NEED_LIBCXXFS)
    set(LIBCXXFS_LIB "stdc++fs")
else ()
    set(LIBCXXFS_LIB "")
endif ()

target_link_libraries(
    dalilib
    ${ACT_LIBRARY} ${ACTVLSILIB_LIBRARY} ${PHYDB_LIBRARY}
    ${Galois_LIBRARIES}
    ${LEF_LIBRARY} ${DEF_LIBRARY}
    ${CPLEX_LIBRARIES} m pthread dl
    OpenMP::OpenMP_CXX
    ${LIBCXXFS_LIB}
)

if (DALI_HAS_QT_GUI)
    add_library(
        daligui
        STATIC
        dali/gui/qt_placement_snapshot_sink.cc
        dali/gui/qt_placement_snapshot_sink.h
    )
    target_link_libraries(daligui PUBLIC dalilib Qt6::Widgets)
    target_compile_definitions(daligui PUBLIC DALI_HAS_QT_GUI QT_NO_KEYWORDS)
endif ()

# ------------------------------------------------------------------------------
# Executable Targets
# ------------------------------------------------------------------------------
macro(add_dali_executable name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE dalilib)
endmacro()

if (DALI_HAS_QT_GUI)
    set(DALI_EXECUTABLE_LIBRARIES daligui)
else ()
    set(DALI_EXECUTABLE_LIBRARIES dalilib)
endif ()

add_executable(dali dali/application/dali_main.cc)
target_link_libraries(dali PRIVATE ${DALI_EXECUTABLE_LIBRARIES})
add_dali_executable(dali-debug dali/application/debug.cc)
add_dali_executable(wellflow dali/application/well_flow.cc)
add_dali_executable(adaptec-debug dali/application/debug_adaptec.cc)
add_dali_executable(hpwl dali/application/hpwl.cc)
add_dali_executable(lefdef2bookshelf dali/application/lef_def_to_bookshelf.cc)
add_dali_executable(bookshelf2lefdef dali/application/bookshelf_to_lef_def.cc)
add_dali_executable(mhlg dali/application/multi_height_legalization.cc)

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests/application)
add_subdirectory(tests/common)
add_subdirectory(tests/circuit)
add_subdirectory(tests/placer)

add_custom_target(
    test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
    USES_TERMINAL
    COMMENT "Run fast unit tests"
)

add_custom_target(
    test-integration
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L integration
    USES_TERMINAL
    COMMENT "Run integration tests"
)

add_custom_target(
    test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    USES_TERMINAL
    COMMENT "Run all configured tests"
)

# ------------------------------------------------------------------------------
# Installation Rules
# ------------------------------------------------------------------------------
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(STATUS "Changing default installation directory to $ACT_HOME")
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
endif ()
message(STATUS "Current installation directory: ${CMAKE_INSTALL_PREFIX}")

# Install header files
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

# Install binaries and libraries
install(TARGETS dali DESTINATION bin)
install(TARGETS dalilib DESTINATION lib)
if (DALI_HAS_QT_GUI)
    install(TARGETS daligui DESTINATION lib)
endif ()
