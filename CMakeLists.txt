cmake_minimum_required(VERSION 3.9)

# We only use C++ compiler for this project.
# By specifying the languages, project() will only check if there is a C++ compiler.
# Default values of LANGUAGES is C and C++, if not specified.
project(Dali
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    module/eigen-git-mirror)

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
find_library(ACTVLSILIB_LIBRARY NAMES vlsilib PATHS $ENV{ACT_HOME}/lib REQUIRED)
find_library(PHYDB_LIBRARY NAMES phydb PATHS $ENV{ACT_HOME}/lib REQUIRED)

############################################################################
# Check if the galois_eda library is installed
############################################################################
find_library(
    GALOIS_EDA_LIBRARY
    NAMES galois_eda
    PATHS $ENV{ACT_HOME}/lib
)
if (NOT GALOIS_EDA)
    message(STATUS "Cannot find libgalois_eda.a")
else ()
    message(STATUS "Found libgalois_eda.a")
endif ()

############################################################################
# detect the installation path of LEF parser and DEF parser,
# and check if the liblef and libdef library can be found or not
############################################################################
message(STATUS "Detecting environment variable LEF_ROOT...")
if (DEFINED ENV{LEF_ROOT})
    message(STATUS "Environment variable LEF_ROOT detected: " $ENV{LEF_ROOT})
else ()
    message(FATAL_ERROR "Environment variable LEF_ROOT not found")
endif ()
include_directories($ENV{LEF_ROOT}/include)
link_directories($ENV{LEF_ROOT}/lib)
find_library(LEF_LIBRARY NAMES lef PATHS $ENV{LEF_ROOT}/lib REQUIRED)

message(STATUS "Detecting environment variable DEF_ROOT...")
if (DEFINED ENV{DEF_ROOT})
    message(STATUS "Environment variable LEF_ROOT detected: " $ENV{DEF_ROOT})
else ()
    message(FATAL_ERROR "Environment variable DEF_ROOT not found")
endif ()
include_directories($ENV{DEF_ROOT}/include)
link_directories($ENV{DEF_ROOT}/lib)
find_library(DEF_LIBRARY NAMES def PATHS $ENV{DEF_ROOT}/lib REQUIRED)

############################################################################
# Detect if OpenMP library exists
############################################################################
find_package(OpenMP REQUIRED)

############################################################################
# Detect if Boost library exists
############################################################################
#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.71.0 COMPONENTS log_setup log filesystem REQUIRED)
message(STATUS "Boost library path: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost include path: ${Boost_INCLUDE_DIR}")
message(STATUS "Boost libs: ${Boost_LIBRARIES}")
include_directories(${Boost_INCLUDE_DIRS})
#link_libraries(${Boost_LIBRARIES})

############################################################################
# for building a standalone static executable
############################################################################
#set(BUILD_STATIC_EXECUTABLES FALSE)
#if(BUILD_STATIC_EXECUTABLES)
#    set(CMAKE_EXE_LINKER_FLAGS -static)
#    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
#    set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
#    set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
#    set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
#    set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
#    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
#    set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
#    # Maybe this works as well, haven't tried yet.
#    # set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
#else(BUILD_STATIC_EXECUTABLES)
#    # Set RPATH to use for installed targets; append linker search path
#    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LOFAR_LIBDIR}")
#    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
#endif(BUILD_STATIC_EXECUTABLES)

# Set C++ compilation flags and options
set(CMAKE_CXX_FLAGS_DEBUG "-g -O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Set a default build type if none was specified
set(default_build_type "RELEASE")
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}")
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "DEBUG" "RELEASE")
endif ()

if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "DEBUG build type")
endif (CMAKE_BUILD_TYPE MATCHES DEBUG)

if (CMAKE_BUILD_TYPE MATCHES RELEASE)
    message(STATUS "RELEASE build type")
endif (CMAKE_BUILD_TYPE MATCHES RELEASE)

if (APPLE)
    add_compile_options(-Xpreprocessor)
endif ()
add_compile_options(-fopenmp)
add_compile_options(-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic)

# Set the output directory of static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Set the output directory of executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# All used circuit and placer source code
file(
    GLOB SOURCES
    dali/dali.cc
    dali/circuit/*.cc
    dali/common/*.cc
    dali/placer/*.cc
    dali/placer/globalPlacer/*.cc
    dali/placer/globalPlacer/GPSimPL/*.cc
    dali/placer/detailedPlacer/*.cc
    dali/placer/detailedPlacer/MDPlacer/*.cc
    dali/placer/ioPlacer/*.cc
    dali/placer/legalizer/*.cc
    dali/placer/legalizer/LGTetris/*.cc
    dali/placer/postLegalOptimizer/*.cc
    dali/placer/postLegalOptimizer/PLOSlide/*.cc
    dali/placer/wellLegalizer/*.cc
    dali/placer/wellPlaceFlow/*.cc
    dali/placer/wellTapPlacer/*.cc
)

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This dalilib library is not necessary, this library is created because we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(
    dalilib
    STATIC
    ${SOURCES}
)
target_link_libraries(
    dalilib
    ${ACT_LIBRARY} ${ACTVLSILIB_LIBRARY} ${PHYDB_LIBRARY} ${GALOIS_EDA_LIBRARY}
    ${LEF_LIBRARY} ${DEF_LIBRARY}
    ${Boost_LIBRARIES}
    OpenMP::OpenMP_CXX
    galois_eda galois_shmem numa
)

# Stand-alone placer, building from the dalilib library
add_executable(
    dali
    dali/dali_standalone.cc
)
target_link_libraries(
    dali
    PRIVATE dalilib
)

# Debugging executable, building from the dalilib library
add_executable(
    dali-debug
    EXCLUDE_FROM_ALL
    dali/debug.cc
)
target_link_libraries(
    dali-debug
    PRIVATE dalilib
)

# Debugging executable, building from the dalilib library
add_executable(
    wellflow
    EXCLUDE_FROM_ALL
    dali/wellflow.cc
)
target_link_libraries(
    wellflow
    PRIVATE dalilib
)

# Debugging executable, building from the dalilib library
add_executable(
    adaptec-debug
    EXCLUDE_FROM_ALL
    dali/debug-adaptec.cc
)
target_link_libraries(
    adaptec-debug
    PRIVATE dalilib
)

# Stand-alone hpwl calculator, building from the dalilib library
add_executable(
    hpwl
    EXCLUDE_FROM_ALL
    dali/utility/hpwl.cc
)
target_link_libraries(
    hpwl
    PRIVATE dalilib
)

# Stand-alone LEFDEF to Bookshelf format tool, building from the dalilib library
add_executable(
    lefdef2bookshelf
    EXCLUDE_FROM_ALL
    dali/utility/lefdef2bookshelf.cc
)
target_link_libraries(
    lefdef2bookshelf
    PRIVATE dalilib
)

# Stand-alone Bookshelf to LEFDEF format tool, building from the dalilib library
add_executable(
    bookshelf2def
    EXCLUDE_FROM_ALL
    dali/utility/bookshelf2def.cc
)
target_link_libraries(
    bookshelf2def
    PRIVATE dalilib
)

# Stand-alone custom cell design to standard cell design tool
add_executable(
    custom2standard
    EXCLUDE_FROM_ALL
    dali/utility/custom2standard.cc
)
target_link_libraries(
    custom2standard
    PRIVATE dalilib
)

# Stand-alone custom cell design to standard cell design tool
add_executable(
    create_circuit
    EXCLUDE_FROM_ALL
    dali/example_code/create_circuit.cc
)
target_link_libraries(
    create_circuit
    PRIVATE dalilib
)

# Unittests
enable_testing()
add_subdirectory(tests/ioplacer)

############################################################################
# Configure install destination directory
############################################################################
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "installation path" FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

############################################################################
# Change FALSE to TRUE if you want to change the installation directory to ${ACT_HOME}
############################################################################
if (TRUE)
    message(STATUS "Changing Installation directory to ACT_HOME")
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
    message(STATUS "Current installation directory: " ${CMAKE_INSTALL_PREFIX})
endif ()

############################################################################
# Install header files
############################################################################
set(INCLUDE_DESTIN include)
install(
    DIRECTORY dali/
    DESTINATION ${INCLUDE_DESTIN}/dali
    COMPONENT Development
    FILES_MATCHING
    PATTERN "*.h"
)
install(
    DIRECTORY dali/../module/eigen-git-mirror/Eigen/
    DESTINATION ${INCLUDE_DESTIN}/dali/Eigen
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
