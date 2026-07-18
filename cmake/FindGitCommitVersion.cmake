# Generate the Git version header at build time so incremental builds cannot
# retain a commit captured by an older CMake configure step.

find_package(Git QUIET)

set(DALI_GIT_VERSION_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/dali/common/git_version.h")
set(DALI_GIT_VERSION_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateGitVersion.cmake")
set(DALI_GIT_VERSION_TEMPLATE
    "${CMAKE_CURRENT_SOURCE_DIR}/dali/common/git_version.h.in")

set(DALI_GIT_VERSION_COMMAND
    ${CMAKE_COMMAND}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DTEMPLATE_FILE=${DALI_GIT_VERSION_TEMPLATE}
    -DOUTPUT_FILE=${DALI_GIT_VERSION_HEADER}
    -P ${DALI_GIT_VERSION_SCRIPT})

# Create the header during configuration for IDEs and dependency scanners.
execute_process(COMMAND ${DALI_GIT_VERSION_COMMAND})

add_custom_target(
    dali_git_version
    COMMAND ${DALI_GIT_VERSION_COMMAND}
    BYPRODUCTS ${DALI_GIT_VERSION_HEADER}
    COMMENT "Refreshing Dali Git version"
    VERBATIM
)

if (GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DALI_CONFIGURED_GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else ()
    set(DALI_CONFIGURED_GIT_VERSION "git version not found")
endif ()
message(STATUS "Git commit hash: ${DALI_CONFIGURED_GIT_VERSION}")
