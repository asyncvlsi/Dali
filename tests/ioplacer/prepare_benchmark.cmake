execute_process(
    COMMAND tar -xvzf "${INPUT_TAR}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE extract_result
)

if(NOT extract_result EQUAL 0)
    message(FATAL_ERROR "Failed to extract benchmark tarball: ${INPUT_TAR}")
endif()

set(lef_file "${WORKDIR}/ispd19_test3.input.lef")
if(NOT EXISTS "${lef_file}")
    message(FATAL_ERROR "Missing extracted LEF file: ${lef_file}")
endif()

file(READ "${lef_file}" lef_contents)
set(old_origin "  ORIGIN 0.06 0 ;")
string(FIND "${lef_contents}" "${old_origin}" origin_pos)
if(origin_pos EQUAL -1)
    message(FATAL_ERROR "Expected pllclk macro origin was not found in ${lef_file}")
endif()

string(REPLACE "${old_origin}" "  ORIGIN 0 0 ;" lef_contents "${lef_contents}")
file(WRITE "${lef_file}" "${lef_contents}")
