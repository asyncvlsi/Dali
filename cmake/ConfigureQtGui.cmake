# Configure Dali's Qt 6 live placement GUI.
#
# There is no switch. Qt 6 Widgets present means Dali supports the GUI, absent
# means it does not. A build option would be a second answer to a question the
# toolchain can already answer, and downstream tools read the answer from
# whether libdaligui was installed -- so an off switch would let them meet a
# machine that has Qt and a Dali that deliberately has no viewer.
#
# Standard CMake package discovery covers system installations on Ubuntu.
# Additional Homebrew hints support common Apple Silicon and Intel prefixes.

set(DALI_HAS_QT_GUI FALSE)

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
    message(STATUS "Qt6 Widgets found: enabling Dali GUI support")
else ()
    message(STATUS "Qt6 Widgets not found: building Dali without GUI support")
endif ()
