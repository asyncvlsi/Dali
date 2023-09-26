# CMake script for generating a Git version header

# This CMake script checks for the existence of the Git binary and generates
# a header file (git_version.h) containing the Git commit hash if Git is
# available, or "git version not found" if Git is not installed.

set(GIT_VERSION_NOT_FOUND "git version not found")
set(GIT_VERSION ${GIT_VERSION_NOT_FOUND})

# Check if git binary exists
execute_process(
        COMMAND command -v git
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_FOUND
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GIT_FOUND)
    execute_process(
            COMMAND git rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

message(STATUS "Git commit hash: ${GIT_VERSION}")

# Configure the version.h header file
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/dali/common/git_version.h.in
        ${CMAKE_CURRENT_SOURCE_DIR}/dali/common/git_version.h
)
