#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Project Settings
#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
cmake_minimum_required(VERSION 3.12)

project(
    Dali
    LANGUAGES CXX
)

option(
    DALI_FORCE_NO_GALOIS
    "Compile Dali's no-Galois fallback even when PhyDB has timing support"
    OFF
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
if (DALI_FORCE_NO_GALOIS)
    set(Galois_LIBRARIES "")
    message(STATUS "Forcing the no-Galois Dali build path")
else ()
    include(cmake/FindGaloisEDA.cmake)
endif ()
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
# Build outputs stay in the build tree.
#
# These used to point into the source tree, so every configuration wrote the
# same ${CMAKE_CURRENT_SOURCE_DIR}/bin and /lib -- and there are routinely three
# at once: a developer's build/, update_toolchain.sh's build-install/, and any
# scratch tree used to check that a no-Qt build still works. Each silently
# replaced the others' binaries and static libraries, so a Qt-enabled dali could
# be overwritten by a no-Qt one with nothing to show for it.
#
# Nothing consumes the source-tree copies: interact links
# $(ACT_HOME)/lib/libdalilib.a and the flows call $(ACT_HOME)/bin, both of which
# come from `cmake --install` rather than from here.
#
# They are still redirected, into the build tree's own bin/ and lib/, because
# the `dali` executable target and the `dali/` object directory collide in the
# build root: the linker is asked to write `dali` where a directory already
# exists. That collision is why the source tree was used in the first place.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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

if (DALI_FORCE_NO_GALOIS)
    # PhyDB's generated config header normally owns this value. Defining its
    # include guard lets a Galois-enabled development install compile the same
    # fallback branch used with a no-Galois PhyDB in CI.
    target_compile_definitions(
        dalilib
        PUBLIC PHYDB_TIMING_CONFIG_H_ PHYDB_USE_GALOIS=0
    )
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
add_subdirectory(tests/timing)
add_subdirectory(tests/gui)

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

add_custom_target(
    check-no-galois-compile
    COMMAND ${CMAKE_COMMAND}
            -S ${CMAKE_SOURCE_DIR}
            -B ${CMAKE_BINARY_DIR}/no-galois-compile
            -DDALI_FORCE_NO_GALOIS=ON
            -DDALI_OR_TOOLS=OFF
            -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    # Everything, not just dalilib. The first CI failure was bin/dali; the
    # second was a test binary calling two helpers that sat behind the
    # timing-host guard, which libdalilib.a alone looked innocent of.
    #
    # Link errors are tolerated here on purpose: a Galois-enabled PhyDB cannot
    # satisfy a forced no-Galois link, and that mixture never occurs on CI. The
    # objects are what the check below needs, and they are produced regardless.
    #
    # Compiling is not linking. The script builds the tree and then checks the
    # part that must hold in every configuration and can be checked without a
    # matching PhyDB.
    COMMAND bash
            ${CMAKE_SOURCE_DIR}/scripts/check_dali_symbols_self_contained.sh
            ${CMAKE_COMMAND}
            ${CMAKE_BINARY_DIR}/no-galois-compile
    #
    # A source condition, not a symbol one: NAN and INFINITY are macros libc++
    # exposes through other headers and libstdc++ does not, so building here can
    # never find a file that relies on it. Text is the only local signal.
    COMMAND bash
            ${CMAKE_SOURCE_DIR}/scripts/check_math_macro_includes.sh
            ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Compile the no-Galois fallback used by the minimal CI toolchain"
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
else ()
    # The installed library is how downstream tools learn whether Dali has a
    # viewer, so one left behind by an earlier build with Qt would keep saying
    # yes after Qt went away.
    install(CODE "file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/lib/libdaligui.a\")")
endif ()
