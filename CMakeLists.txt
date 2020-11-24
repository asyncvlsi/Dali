cmake_minimum_required(VERSION 3.9)

# We only use C++ compiler for this project.
# By specifying the languages, project() will only check if there is a C++ compiler.
# Default values of LANGUAGES is C and C++, if not specified.
project(Dali
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

######################################
# Detect if Boost library exists
######################################
find_package(Boost 1.71.0 COMPONENTS filesystem log_setup log REQUIRED)
message(STATUS "Boost library path: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost include path: ${Boost_INCLUDE_DIR}")
message(STATUS "Boost libs: ${Boost_LIBRARIES}")
include_directories(${Boost_INCLUDE_DIRS})
link_libraries(${Boost_LIBRARIES})

######################################
# Detect if OpenMP library exists
######################################
find_package(OpenMP REQUIRED)

######################################
# Detect if $ACT_HOME exits
######################################
message(STATUS "Detecting environment variable ACT_HOME...")
if (DEFINED ENV{ACT_HOME})
    message(STATUS "Environment variable ACT_HOME detected: " $ENV{ACT_HOME})
else ()
    message(FATAL_ERROR "Environment variable ACT_HOME not found")
endif ()
include_directories($ENV{ACT_HOME}/include)
link_directories($ENV{ACT_HOME}/lib)
find_library(ACT_LIBRARY
        NAMES act vlsilib
        PATHS $ENV{ACT_HOME}/lib
        REQUIRED)
if (NOT ACT_LIBRARY)
    message(FATAL_ERROR "libact.a is not found")
else ()
    message(STATUS "libact.a found")
endif()
link_libraries(libact.a libvlsilib.a)

######################################
# Detect if $GALOIS_INSTALL_DIR exists, which is the path to Galois installation
######################################
#message(STATUS "Detecting environment variable GALOIS_INSTALL_DIR...")
#if (DEFINED ENV{GALOIS_INSTALL_DIR})
#    message(STATUS "Environment variable GALOIS_INSTALL detected: " $ENV{GALOIS_INSTALL_DIR})
#else ()
#    message(STATUS "Environment variable GALOIS_INSTALL_DIR not found")
#endif ()
#list(APPEND CMAKE_PREFIX_PATH $ENV{GALOIS_INSTALL_DIR})
#find_package(Galois REQUIRED)
#link_libraries(Galois::shmem)

######################################
# detect the installation path of LEF parser and DEF parser,
# and check if the liblef and libdef library can be found or not
######################################
message(STATUS "Detecting environment variable LEF_ROOT...")
if (DEFINED ENV{LEF_ROOT})
    message(STATUS "Environment variable LEF_ROOT detected: " $ENV{LEF_ROOT})
else ()
    message(FATAL_ERROR "Environment variable LEF_ROOT not found")
endif ()
include_directories($ENV{LEF_ROOT}/include)
link_directories($ENV{LEF_ROOT}/lib)
find_library(LEF_LIBRARY
        NAMES lef
        PATHS $ENV{LEF_ROOT}/lib
        REQUIRED)
if (NOT LEF_LIBRARY)
    message(FATAL_ERROR "liblef.a is not found")
else ()
    message(STATUS "liblef.a found")
endif()
link_libraries(liblef.a)

message(STATUS "Detecting environment variable DEF_ROOT...")
if (DEFINED ENV{DEF_ROOT})
    message(STATUS "Environment variable LEF_ROOT detected: " $ENV{DEF_ROOT})
else ()
    message(FATAL_ERROR "Environment variable DEF_ROOT not found")
endif ()
include_directories($ENV{DEF_ROOT}/include)
link_directories($ENV{DEF_ROOT}/lib)
find_library(DEF_LIBRARY
        NAMES def
        PATHS $ENV{DEF_ROOT}/lib
        REQUIRED)
if (NOT DEF_LIBRARY)
    message(FATAL_ERROR "libdef.a is not found")
else ()
    message(STATUS "libdef.a found")
endif()
link_libraries(libdef.a)


# Set C++ compilation flags and options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3 -fopenmp -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -std=c++17 -O3 -fopenmp -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17 -O3 -fopenmp -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")

# Set the output directory of static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Set the output directory of executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Set a default build type if none was specified
set(default_build_type "Release")
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}")
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        module/eigen-git-mirror
)

# All used circuit and placer source code
file(GLOB SOURCES
        dali/common/*.cc
        dali/circuit/*.cc
        dali/placer/*.cc
        dali/placer/globalPlacer/*.cc
        dali/placer/globalPlacer/GPSimPL/*.cc
        dali/placer/detailedPlacer/*.cc
        dali/placer/detailedPlacer/MDPlacer/*.cc
        dali/placer/legalizer/*.cc
        dali/placer/legalizer/LGTetris/*.cc
        dali/placer/postLegalOptimizer/*.cc
        dali/placer/postLegalOptimizer/PLOSlide/*.cc
        dali/placer/wellLegalizer/*.cc
        dali/placer/wellPlaceFlow/*.cc
        )

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This dalilib library is not necessary, this library is created because we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(dalilib
        STATIC
        ${SOURCES})

# Stand-alone placer, building from the dalilib library
add_executable(dali
        dali/dali.cc)
target_link_libraries(dali
        PRIVATE dalilib
        ${Boost_LIBRARIES})

# Debugging executable, building from the dalilib library
add_executable(dali-debug
        dali/debug.cc)
target_link_libraries(dali-debug
        PRIVATE dalilib
        OpenMP::OpenMP_CXX)

# Debugging executable, building from the dalilib library
add_executable(wellflow
        dali/wellflow.cc)
target_link_libraries(wellflow
        PRIVATE dalilib
        )

# Debugging executable, building from the dalilib library
add_executable(adaptec-debug
        dali/debug-adaptec.cc)
target_link_libraries(adaptec-debug
        PRIVATE dalilib
        )

# Stand-alone hpwl calculator, building from the dalilib library
add_executable(hpwl
        dali/utility/hpwl.cc)
target_link_libraries(hpwl
        PRIVATE dalilib
        )

# Stand-alone LEFDEF to Bookshelf format tool, building from the dalilib library
add_executable(lefdef2bookshelf
        dali/utility/lefdef2bookshelf.cc)
target_link_libraries(lefdef2bookshelf
        PRIVATE dalilib
        )

# Stand-alone Bookshelf to LEFDEF format tool, building from the dalilib library
add_executable(bookshelf2def
        dali/utility/bookshelf2def.cc)
target_link_libraries(bookshelf2def
        PRIVATE dalilib
        )

# Stand-alone custom cell design to standard cell design tool
add_executable(custom2standard
        dali/utility/custom2standard.cc)
target_link_libraries(custom2standard
        PRIVATE dalilib
        )

# Stand-alone custom cell design to standard cell design tool
add_executable(1create_circuit
        dali/example_code/1create_circuit.cc)
target_link_libraries(1create_circuit
        PRIVATE dalilib
        )

######################################
# Configure install destination directory
######################################
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "installation path" FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

######################################
# Change FALSE to TRUE if you want to change the installation directory to ${ACT_HOME}
######################################
if (FALSE)
    message(STATUS "Changing Installation directory to ACT_HOME")
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
    message(STATUS "Current installation directory: " ${CMAKE_INSTALL_PREFIX})
endif ()

######################################
# Install header files
######################################
set(INCLUDE_DESTIN include)
install(DIRECTORY dali/ DESTINATION ${INCLUDE_DESTIN}/dali COMPONENT Development FILES_MATCHING PATTERN "*.h")

######################################
# Install binary
######################################
install(TARGETS dali
    DESTINATION bin)

######################################
# Install library
######################################
install(TARGETS dalilib
        DESTINATION lib)
