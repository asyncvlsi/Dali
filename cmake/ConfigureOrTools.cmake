# Configure Dali's optional OR-Tools CP-SAT backend.
#
# Discovery uses the normal CMake package search first, so an OR-Tools package
# installed under /usr or /usr/local on Ubuntu works without a platform-specific
# path. ORTOOLS_ROOT and the environment variable ORTOOLS_ROOT provide explicit
# prefix hints for unpacked binary distributions and custom installations.

set(DALI_OR_TOOLS "AUTO" CACHE STRING
    "OR-Tools legalization backend: AUTO, ON, or OFF")
set_property(CACHE DALI_OR_TOOLS PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${DALI_OR_TOOLS}" DALI_OR_TOOLS_MODE)

if (NOT DALI_OR_TOOLS_MODE MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR
            "DALI_OR_TOOLS must be AUTO, ON, or OFF; got '${DALI_OR_TOOLS}'")
endif ()

set(DALI_OR_TOOLS_MIN_VERSION "9.15")
set(DALI_OR_TOOLS_NEXT_VERSION "9.16")
set(DALI_HAS_OR_TOOLS FALSE)

if (DALI_OR_TOOLS_MODE STREQUAL "OFF")
    message(STATUS "OR-Tools support disabled by DALI_OR_TOOLS=OFF")
    return()
endif ()

set(dali_ortools_hints)
if (ORTOOLS_ROOT)
    list(APPEND dali_ortools_hints "${ORTOOLS_ROOT}")
endif ()
if (DEFINED ENV{ORTOOLS_ROOT})
    list(APPEND dali_ortools_hints "$ENV{ORTOOLS_ROOT}")
endif ()

# Homebrew's prefix is not always in CMake's default package search path.
# These hints cover Apple Silicon and Intel Homebrew without affecting Linux.
if (APPLE)
    list(APPEND dali_ortools_hints
         /opt/homebrew/opt/or-tools
         /usr/local/opt/or-tools)
endif ()

# Common locations for an installed or unpacked Linux binary distribution.
if (UNIX AND NOT APPLE)
    list(APPEND dali_ortools_hints
         /usr
         /usr/local
         /opt/or-tools)
endif ()

# OR-Tools' package configuration resolves its own dependencies with nested
# find_package() calls. Adding the candidate prefixes here lets those calls find
# dependencies installed beside OR-Tools in a custom Ubuntu installation.
list(APPEND CMAKE_PREFIX_PATH ${dali_ortools_hints})

# Some binary packages export the SCIP target as libscip while OR-Tools first
# checks for the namespaced alias. Define the alias before loading OR-Tools to
# keep package discovery quiet and preserve the target expected by its link
# interface.
find_package(SCIP CONFIG QUIET)
if (TARGET libscip AND NOT TARGET SCIP::libscip)
    add_library(SCIP::libscip ALIAS libscip)
endif ()

find_package(ortools ${DALI_OR_TOOLS_MIN_VERSION} CONFIG QUIET
             HINTS ${dali_ortools_hints})

# A package configuration can be located but still fail while resolving its
# Abseil, Protobuf, or solver dependencies. The imported target is the reliable
# indication that the complete C++ package is usable.
if (TARGET ortools::ortools)
    if (ORTOOLS_VERSION VERSION_LESS DALI_OR_TOOLS_NEXT_VERSION)
        # OR-Tools 9.15 binary packages export two unused MathOpt interface
        # targets whose object-library sources are not installed. New CMake
        # versions evaluate those references even when Dali links only CP-SAT.
        # Dali does not use MathOpt, so remove the invalid source properties.
        foreach(dali_unused_mathopt_target
                ortools::ortools_math_opt
                ortools::ortools_math_opt_constraints)
            if (TARGET ${dali_unused_mathopt_target})
                set_property(TARGET ${dali_unused_mathopt_target}
                             PROPERTY INTERFACE_SOURCES "")
            endif ()
        endforeach()
        set(DALI_HAS_OR_TOOLS TRUE)
        message(STATUS
                "OR-Tools ${ORTOOLS_VERSION} found: enabling CP-SAT legalization support")
        return()
    endif ()
    set(dali_ortools_failure_reason
        "found unsupported OR-Tools ${ORTOOLS_VERSION}; Dali requires 9.15.x")
elseif (ortools_DIR OR DEFINED ORTOOLS_VERSION)
    find_program(dali_pkg_config_executable NAMES pkg-config)
    if (dali_pkg_config_executable)
        set(dali_ortools_failure_reason
            "found the OR-Tools package configuration, but one or more transitive dependencies could not be loaded")
    elseif (APPLE)
        set(dali_ortools_failure_reason
            "found the OR-Tools package configuration, but pkg-config is unavailable; install it with 'brew install pkgconf'")
    else ()
        set(dali_ortools_failure_reason
            "found the OR-Tools package configuration, but pkg-config is unavailable; install the Ubuntu package 'pkg-config'")
    endif ()
else ()
    set(dali_ortools_failure_reason
        "OR-Tools 9.15.x was not found; set ORTOOLS_ROOT or ortools_DIR for a custom installation")
endif ()

if (DALI_OR_TOOLS_MODE STREQUAL "ON")
    message(FATAL_ERROR
            "DALI_OR_TOOLS=ON cannot enable OR-Tools: ${dali_ortools_failure_reason}")
else ()
    message(STATUS
            "${dali_ortools_failure_reason}; building without CP-SAT legalization support")
endif ()
