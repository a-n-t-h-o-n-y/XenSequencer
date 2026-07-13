if (NOT DEFINED PATCH_FILE OR NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "PATCH_FILE must name an existing patch file")
endif()

if (NOT DEFINED SOURCE_DIR OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "SOURCE_DIR must name an existing source directory")
endif()

set(GIT_APPLY_OPTIONS --ignore-space-change)

execute_process(
    COMMAND git apply --reverse --check ${GIT_APPLY_OPTIONS} "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE PATCH_ALREADY_APPLIED
    OUTPUT_QUIET
    ERROR_QUIET
)

if (PATCH_ALREADY_APPLIED EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH_FILE}")
    return()
endif()

execute_process(
    COMMAND git apply --check ${GIT_APPLY_OPTIONS} "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE PATCH_CHECK_RESULT
    ERROR_VARIABLE PATCH_CHECK_ERROR
)

if (NOT PATCH_CHECK_RESULT EQUAL 0)
    message(FATAL_ERROR "Patch does not apply cleanly: ${PATCH_FILE}\n${PATCH_CHECK_ERROR}")
endif()

execute_process(
    COMMAND git apply --whitespace=nowarn ${GIT_APPLY_OPTIONS} "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE PATCH_APPLY_RESULT
    ERROR_VARIABLE PATCH_APPLY_ERROR
)

if (NOT PATCH_APPLY_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to apply patch: ${PATCH_FILE}\n${PATCH_APPLY_ERROR}")
endif()
