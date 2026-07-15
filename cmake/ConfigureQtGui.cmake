# Configure Dali's optional Qt 6 live placement GUI.
#
# Standard CMake package discovery covers system installations on Ubuntu.
# Additional Homebrew hints support common Apple Silicon and Intel prefixes.

set(DALI_GUI "AUTO" CACHE STRING "Qt live placement GUI mode: AUTO, ON, or OFF")
set_property(CACHE DALI_GUI PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${DALI_GUI}" DALI_GUI_MODE)

if (NOT DALI_GUI_MODE MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR
            "DALI_GUI must be AUTO, ON, or OFF; got '${DALI_GUI}'")
endif ()

set(DALI_HAS_QT_GUI FALSE)
if (DALI_GUI_MODE STREQUAL "OFF")
    message(STATUS "Qt GUI support disabled by DALI_GUI=OFF")
    return()
endif ()

if (APPLE)
    foreach(dali_qt_prefix
            /opt/homebrew/opt/qt
            /opt/homebrew/opt/qt@6
            /usr/local/opt/qt
            /usr/local/opt/qt@6)
        if (EXISTS "${dali_qt_prefix}/lib/cmake/Qt6")
            list(APPEND CMAKE_PREFIX_PATH "${dali_qt_prefix}")
        endif ()
    endforeach()

    find_program(dali_brew_executable brew)
    if (dali_brew_executable)
        foreach(dali_qt_formula qt qt@6)
            execute_process(
                COMMAND ${dali_brew_executable} --prefix ${dali_qt_formula}
                RESULT_VARIABLE dali_brew_qt_result
                OUTPUT_VARIABLE dali_brew_qt_prefix
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if (dali_brew_qt_result EQUAL 0 AND
                EXISTS "${dali_brew_qt_prefix}/lib/cmake/Qt6")
                list(APPEND CMAKE_PREFIX_PATH "${dali_brew_qt_prefix}")
            endif ()
        endforeach()
    endif ()
endif ()

find_package(Qt6 COMPONENTS Widgets QUIET)
if (TARGET Qt6::Widgets)
    set(DALI_HAS_QT_GUI TRUE)
    message(STATUS "Qt6 Widgets found: enabling Dali GUI debug support")
elseif (DALI_GUI_MODE STREQUAL "ON")
    message(FATAL_ERROR
            "DALI_GUI=ON requires Qt6 Widgets; set Qt6_ROOT or CMAKE_PREFIX_PATH for a custom installation")
else ()
    message(STATUS
            "Qt6 Widgets not found: building Dali without GUI debug support")
endif ()
