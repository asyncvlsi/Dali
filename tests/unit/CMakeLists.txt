cmake_minimum_required(VERSION 3.12)

function(add_dali_unit_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} PRIVATE dalilib)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_dali_unit_test(misc_test misc_test.cc)
add_dali_unit_test(logging_parallel_test logging_parallel_test.cc)
