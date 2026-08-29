# Qt tests exist only where Qt does. A machine without Qt still builds and runs
# everything else, which is the property most easily broken here.
if (NOT DALI_HAS_QT_GUI)
    message(STATUS "Qt GUI disabled; skipping tests/gui")
    return()
endif ()

find_package(GTest QUIET)
if (NOT GTest_FOUND)
    message(STATUS "GoogleTest not found; skipping tests/gui")
    return()
endif()

if (TARGET GTest::gtest_main)
    set(DALI_GTEST_MAIN GTest::gtest_main)
elseif (TARGET GTest::Main)
    set(DALI_GTEST_MAIN GTest::Main)
else()
    message(STATUS "GoogleTest main target not found; skipping tests/gui")
    return()
endif()

add_executable(qt_timing_diagnostics_panel_test
    qt_timing_diagnostics_panel_test.cc)
target_link_libraries(qt_timing_diagnostics_panel_test
    PRIVATE dalilib daligui ${DALI_GTEST_MAIN})
add_test(NAME qt_timing_diagnostics_panel_test
    COMMAND qt_timing_diagnostics_panel_test)

add_executable(qt_placement_window_test
    qt_placement_window_test.cc)
target_link_libraries(qt_placement_window_test
    PRIVATE dalilib daligui ${DALI_GTEST_MAIN})
add_test(NAME qt_placement_window_test
    COMMAND qt_placement_window_test)

# Headless: these open no window on screen, but Qt still needs a platform
# plugin, and the offscreen one paints for real so grabs are meaningful.
set_tests_properties(qt_timing_diagnostics_panel_test qt_placement_window_test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
