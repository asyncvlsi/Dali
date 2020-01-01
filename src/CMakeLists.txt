# Set a default build type if none was specified
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}")
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

include_directories(
    ../src
    ../module/OpenDB/include/lef56
    ../module/OpenDB/include/opendb
)

# All used circuit and placer source code
file(GLOB SOURCES
    opendb.cc
    common/*.cc
    circuit/*.cc
    placer/*.cc
    placer/globalPlacer/*.cc
    placer/globalPlacer/GPSimPL/*.cc
    placer/detailedPlacer/*.cc
    placer/detailedPlacer/DPLinear/*.cc
    placer/detailedPlacer/MDPlacer/*.cc
    placer/legalizer/*.cc
    placer/legalizer/LGTetris/*.cc
    placer/wellLegalizer/*.cc
    )

# Create a library, static libraries are controlled by ARCHIVE_OUTPUT_DIRECTORY property
# Shared libraries are controlled by LIBRARY_OUTPUT_DIRECTORY
# This placer library is not necessary, this library is created because we need to put it somewhere in $ACT_HOME or $CAD_HOME
add_library(placer
    STATIC
    ${SOURCES})

# Stand-alone placer, building from the placer library
add_executable(dali
    dali.cc)
target_link_libraries(dali
    PRIVATE placer
    opendb)

# Debugging exectuable, building from the placer library
add_executable(dali-debug
    EXCLUDE_FROM_ALL
    debug.cc)
target_link_libraries(dali-debug
    PRIVATE placer
    opendb)

# Stand-alone hpwl calculator, building from the placer library
add_executable(hpwl
    EXCLUDE_FROM_ALL
    hpwl.cc)
target_link_libraries(hpwl
    PRIVATE placer
    opendb)

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
  if(FALSE)
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
