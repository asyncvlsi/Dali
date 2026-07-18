# Inputs: GIT_EXECUTABLE, SOURCE_DIR, TEMPLATE_FILE, and OUTPUT_FILE.

set(GIT_VERSION_NOT_FOUND "git version not found")
set(GIT_VERSION ${GIT_VERSION_NOT_FOUND})

if (GIT_EXECUTABLE AND EXISTS "${GIT_EXECUTABLE}")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        RESULT_VARIABLE GIT_REVISION_RESULT
        OUTPUT_VARIABLE GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if (GIT_REVISION_RESULT EQUAL 0)
        set(GIT_VERSION ${GIT_REVISION})
        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
                    --untracked-files=no
            WORKING_DIRECTORY ${SOURCE_DIR}
            OUTPUT_VARIABLE GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if (GIT_STATUS)
            string(APPEND GIT_VERSION "-dirty")
        endif ()
    endif ()
endif ()

configure_file(${TEMPLATE_FILE} ${OUTPUT_FILE} @ONLY)
