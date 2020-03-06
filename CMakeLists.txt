cmake_minimum_required(VERSION 3.9)

# We only use C++ compiler for this project.
# By specifying the languages, project() will only check if there is a C++ compiler.
# Default values of LANGUAGES is C and C++, if not specified.
project(Dali
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

set(macro_names "-D USE_OPENDB")

# The flag USE_OPENDB
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${macro_names} -std=c++11 -O3 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g ${macro_names} -std=c++11 -O3 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${macro_names} -std=c++11 -O3 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic")

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

add_subdirectory(module/OpenDB
        EXCLUDE_FROM_ALL)

include_directories(
        src
        module/eigen-git-mirror
        module/OpenDB/include/lef56
        module/OpenDB/include/opendb
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
        src/placer/postLegalOptimizer/*.cc
        src/placer/postLegalOptimizer/PLOSlide/*.cc
        src/placer/wellLegalizer/*.cc
        src/placer/wellPlaceFlow/*.cc
        )

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This placer library is not necessary, this library is created because we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(placer
        STATIC
        ${SOURCES})

# Stand-alone placer, building from the placer library
add_executable(dali
        src/dali.cc)
target_link_libraries(dali
        PRIVATE placer
        opendb)

# Debugging executable, building from the placer library
add_executable(dali-debug
        EXCLUDE_FROM_ALL
        src/debug.cc)
target_link_libraries(dali-debug
        PRIVATE placer
        opendb)

# Debugging executable, building from the placer library
add_executable(wellflow
        EXCLUDE_FROM_ALL
        src/wellflow.cc)
target_link_libraries(wellflow
        PRIVATE placer
        opendb)

# Debugging executable, building from the placer library
add_executable(adaptec-debug
        EXCLUDE_FROM_ALL
        src/debug-adaptec.cc)
target_link_libraries(adaptec-debug
        PRIVATE placer
        opendb)

# Stand-alone hpwl calculator, building from the placer library
add_executable(hpwl
        EXCLUDE_FROM_ALL
        src/utility/hpwl.cc)
target_link_libraries(hpwl
        PRIVATE placer
        opendb)

# Stand-alone LEFDEF to Bookshelf format tool, building from the placer library
add_executable(lefdef2bookshelf
        EXCLUDE_FROM_ALL
        src/utility/lefdef2bookshelf.cc)
target_link_libraries(lefdef2bookshelf
        PRIVATE placer
        opendb)

# Stand-alone Bookshelf to LEFDEF format tool, building from the placer library
add_executable(bookshelf2def
        EXCLUDE_FROM_ALL
        src/utility/bookshelf2def.cc)
target_link_libraries(bookshelf2def
        PRIVATE placer
        opendb)

# Stand-alone custom cell design to standard cell design tool
add_executable(custom2standard
        EXCLUDE_FROM_ALL
        src/utility/custom2standard.cc)
target_link_libraries(custom2standard
        PRIVATE placer
        opendb)

######################################
# Configure install destination directory
######################################
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "installation path" FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

######################################
# Detect if $ACT_HOME exits, change the installation directory to ACT_HOME
######################################
message(STATUS "Detecting environment variable ACT_HOME...")
if (DEFINED ENV{ACT_HOME})
    message(STATUS "Environment variable ACT_HOME detected: " $ENV{ACT_HOME})
    if (FALSE)
        message(STATUS "Changing Installation directory to ACT_HOME")
        set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
        message(STATUS "Current installation directory: " ${CMAKE_INSTALL_PREFIX})
    endif ()
else ()
    message(STATUS "Environment variable ACT_HOME not found")
endif ()

######################################
# Install header files
######################################
set(INLUDE_DESTIN include)
install(DIRECTORY src/ DESTINATION ${INLUDE_DESTIN}/dali COMPONENT Development FILES_MATCHING PATTERN "*.h")

######################################
# Install binary
######################################
#install(TARGETS dali
#    DESTINATION bin)

######################################
# Install library
######################################
install(TARGETS placer
        DESTINATION lib)
