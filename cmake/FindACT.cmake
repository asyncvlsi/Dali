# Locate the ACT toolchain (libact and its companion libraries) via ACT_HOME.
#
# Requires the ACT_HOME environment variable to point at an ACT installation.
# Adds its include/lib directories to the build and exposes the library paths:
#   ACT_LIBRARY, ACTVLSILIB_LIBRARY, PHYDB_LIBRARY, LEF_LIBRARY, DEF_LIBRARY

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
