############################################################################
# Check if the CPLEX libraries are installed
#
# This cmake file will define the following variables
#    CPLEX_FOUND, whether the CPLEX core libraries can be found
#    CPLEX_LIBRARIES, the list of CPLEX libraries
############################################################################
set(CPLEX_FOUND TRUE)
if(DEFINED ENV{CPLEX_INC})
    message(STATUS "Environment variable CPLEX_INC detected: " $ENV{CPLEX_INC})
else()
    message(STATUS "Environment variable CPLEX_INC not found")
    set(CPLEX_FOUND FALSE)
endif()
if(DEFINED ENV{CPLEX_LIB})
    message(STATUS "Environment variable CPLEX_LIB detected: " $ENV{CPLEX_LIB})
else()
    message(STATUS "Environment variable CPLEX_LIB not found")
    set(CPLEX_FOUND FALSE)
endif()
if(DEFINED ENV{CONCERT_INC})
    message(STATUS "Environment variable CONCERT_INC detected: " $ENV{CONCERT_INC})
else()
    message(STATUS "Environment variable CONCERT_INC not found")
    set(CPLEX_FOUND FALSE)
endif()
if(DEFINED ENV{CONCERT_LIB})
    message(STATUS "Environment variable CONCERT_LIB detected: " $ENV{CONCERT_LIB})
else()
    message(STATUS "Environment variable CONCERT_LIB not found")
    set(CPLEX_FOUND FALSE)
endif()

if(CPLEX_FOUND)
    set(DALI_USE_CPLEX 1)
    include_directories($ENV{CPLEX_INC})
    include_directories($ENV{CONCERT_INC})
    add_definitions(-DIL_STD)

    # Find and append ilocplex library
    find_library(
        ILOCPLEX_LIBRARY
        NAMES ilocplex
        PATHS $ENV{CPLEX_LIB}
        REQUIRED
    )
    set(CPLEX_LIBRARIES ${ILOCPLEX_LIBRARY})

    # Find and append concert library
    find_library(
        CONCERT_LIBRARY
        NAMES concert
        PATHS $ENV{CONCERT_LIB}
        REQUIRED
    )
    set(CPLEX_LIBRARIES ${CPLEX_LIBRARIES} ${CONCERT_LIBRARY})

    # Find and append cplex library
    find_library(
        CPLEX_LIBRARY
        NAMES cplex
        PATHS $ENV{CPLEX_LIB}
        REQUIRED
    )
    set(CPLEX_LIBRARIES ${CPLEX_LIBRARIES} ${CPLEX_LIBRARY})

    message(STATUS "CPLEX libs: ${CPLEX_LIBRARIES}")
else()
    set(DALI_USE_CPLEX 0)
endif()

# Configure the config.h header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dali/common/config.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/dali/common/config.h
)
