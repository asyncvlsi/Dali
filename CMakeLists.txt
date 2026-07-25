#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Project Settings
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
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
#Installation Prefix & RPATH
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Resolve the prefix before it is used: CMAKE_INSTALL_RPATH seeds each target's
#INSTALL_RPATH as the target is created, so both must precede the targets.
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(STATUS "Changing default installation directory to $ACT_HOME")
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
endif ()
message(STATUS "Current installation directory: ${CMAKE_INSTALL_PREFIX}")

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Dependency Detection : ACT_HOME, Galois, OpenMP, Git Commit
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
include(cmake/FindACT.cmake)
include(cmake/FindGaloisEDA.cmake)
include(cmake/FindOpenMPPackage.cmake)
include(cmake/FindGitCommitVersion.cmake)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Build Types and Options
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
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

add_compile_options(-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Werror=return-type -pedantic)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Optional Features : OR-Tools, Qt GUI
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
include(cmake/ConfigureOrTools.cmake)
include(cmake/ConfigureQtGui.cmake)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Output Directories
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Library Targets
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Glob all used circuit and placer source code
file(
    GLOB_RECURSE SOURCES
    dali/dali.cc
    dali/application/dali_command_line.cc
    dali/command/*.cc
    dali/command/*.h
    dali/circuit/*.cc
    dali/circuit/*.h
    dali/common/*.cc
    dali/common/*.h
    dali/placer/*.cc
    dali/placer/*.h
    dali/timing/*.cc
    dali/timing/*.h
)

add_library(
    dalilib
    STATIC
    ${SOURCES}
)
add_dependencies(dalilib dali_git_version)

if (Galois_FOUND)
    target_include_directories(dalilib PRIVATE ${Boost_INCLUDE_DIRS})
endif ()

if (DEFINED NEED_LIBCXXFS)
    set(LIBCXXFS_LIB "stdc++fs")
else ()
    set(LIBCXXFS_LIB "")
endif ()

if (DALI_HAS_OR_TOOLS)
    set(DALI_OPTIONAL_LIBRARIES ortools::ortools)
    target_compile_definitions(dalilib PRIVATE DALI_HAS_OR_TOOLS)
else ()
    set(DALI_OPTIONAL_LIBRARIES "")
endif ()

target_link_libraries(
    dalilib
    ${ACT_LIBRARY} ${ACTVLSILIB_LIBRARY} ${PHYDB_LIBRARY}
    ${Boost_LIBRARIES}
    ${Galois_LIBRARIES}
    ${LEF_LIBRARY} ${DEF_LIBRARY}
    m pthread dl
    OpenMP::OpenMP_CXX
    ${LIBCXXFS_LIB}
    ${DALI_OPTIONAL_LIBRARIES}
)

if (DALI_HAS_QT_GUI)
    file(GLOB_RECURSE GUI_SOURCES dali/gui/*.cc dali/gui/*.h)
    add_library(
        daligui
        STATIC
        ${GUI_SOURCES}
    )
    target_link_libraries(daligui PUBLIC dalilib Qt6::Widgets)
    target_compile_definitions(daligui PUBLIC DALI_HAS_QT_GUI QT_NO_KEYWORDS)
endif ()

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Executable Targets
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
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
add_dali_executable(hpwl dali/application/hpwl.cc)
add_dali_executable(lefdef2bookshelf dali/application/lef_def_to_bookshelf.cc)
add_dali_executable(bookshelf2lefdef dali/application/bookshelf_to_lef_def.cc)
add_dali_executable(mhlg dali/application/multi_height_legalization.cc)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Tests
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
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

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Installation Rules
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
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
