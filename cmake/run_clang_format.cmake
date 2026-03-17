if (NOT DEFINED CLANG_FORMAT_EXECUTABLE)
    message(FATAL_ERROR "`CLANG_FORMAT_EXECUTABLE` is required")
endif ()

if (NOT DEFINED CLICKHOUSE_SOURCE_DIR)
    message(FATAL_ERROR "`CLICKHOUSE_SOURCE_DIR` is required")
endif ()

if (NOT DEFINED CLANG_FORMAT_BINARY_DIR)
    message(FATAL_ERROR "`CLANG_FORMAT_BINARY_DIR` is required")
endif ()

if (NOT DEFINED CLANG_FORMAT_MODE)
    set(CLANG_FORMAT_MODE format)
endif ()

if (NOT EXISTS "${CLICKHOUSE_SOURCE_DIR}/.git")
    message(FATAL_ERROR "Cannot find `.git` in `${CLICKHOUSE_SOURCE_DIR}`")
endif ()

if (DEFINED CLANG_FORMAT_INPUT_FILE)
    if (NOT EXISTS "${CLANG_FORMAT_INPUT_FILE}")
        message(FATAL_ERROR "Cannot find `CLANG_FORMAT_INPUT_FILE`: ${CLANG_FORMAT_INPUT_FILE}")
    endif ()

    file(STRINGS "${CLANG_FORMAT_INPUT_FILE}" CLANG_FORMAT_FILES)
else ()
    execute_process(
        COMMAND git
            -C "${CLICKHOUSE_SOURCE_DIR}"
            ls-files
            --
            *.c
            *.cc
            *.cpp
            *.cxx
            *.h
            *.hh
            *.hpp
            *.hxx
            *.inc
            *.ipp
            *.tpp
        OUTPUT_VARIABLE GIT_LS_FILES_OUTPUT
        RESULT_VARIABLE GIT_LS_FILES_RESULT
        ERROR_VARIABLE GIT_LS_FILES_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if (NOT GIT_LS_FILES_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to enumerate tracked files with `git ls-files`: ${GIT_LS_FILES_ERROR}")
    endif ()

    string(REPLACE "\n" ";" GIT_LS_FILES_LIST "${GIT_LS_FILES_OUTPUT}")

    set(CLANG_FORMAT_FILES)
    foreach(relative_file IN LISTS GIT_LS_FILES_LIST)
        if (relative_file STREQUAL "")
            continue()
        endif ()

        if (relative_file MATCHES "^contrib/")
            continue()
        endif ()

        list(APPEND CLANG_FORMAT_FILES "${CLICKHOUSE_SOURCE_DIR}/${relative_file}")
    endforeach ()
endif ()

list(LENGTH CLANG_FORMAT_FILES CLANG_FORMAT_FILE_COUNT)
if (CLANG_FORMAT_FILE_COUNT EQUAL 0)
    message(STATUS "No tracked C/C++ files matched for `clang-format`")
    return()
endif ()

set(CLANG_FORMAT_TMP_DIR "${CLANG_FORMAT_BINARY_DIR}/tmp")
set(CLANG_FORMAT_FILE_LIST "${CLANG_FORMAT_TMP_DIR}/clang-format-files.txt")
file(MAKE_DIRECTORY "${CLANG_FORMAT_TMP_DIR}")
string(REPLACE ";" "\n" CLANG_FORMAT_FILE_LIST_CONTENT "${CLANG_FORMAT_FILES}")
file(WRITE "${CLANG_FORMAT_FILE_LIST}" "${CLANG_FORMAT_FILE_LIST_CONTENT}\n")

set(CLANG_FORMAT_COMMAND
    "${CLANG_FORMAT_EXECUTABLE}"
    --style=file
    "--files=${CLANG_FORMAT_FILE_LIST}")

if (CLANG_FORMAT_MODE STREQUAL "check")
    list(APPEND CLANG_FORMAT_COMMAND --dry-run --Werror)
elseif (CLANG_FORMAT_MODE STREQUAL "format")
    list(APPEND CLANG_FORMAT_COMMAND -i)
else ()
    message(FATAL_ERROR "Unsupported `CLANG_FORMAT_MODE`: ${CLANG_FORMAT_MODE}")
endif ()

message(STATUS "Running `clang-format` in `${CLANG_FORMAT_MODE}` mode on ${CLANG_FORMAT_FILE_COUNT} tracked files")

execute_process(
    COMMAND ${CLANG_FORMAT_COMMAND}
    WORKING_DIRECTORY "${CLICKHOUSE_SOURCE_DIR}"
    RESULT_VARIABLE CLANG_FORMAT_RESULT
    OUTPUT_VARIABLE CLANG_FORMAT_STDOUT
    ERROR_VARIABLE CLANG_FORMAT_STDERR)

if (NOT CLANG_FORMAT_RESULT EQUAL 0)
    message(FATAL_ERROR
        "`clang-format` failed with exit code ${CLANG_FORMAT_RESULT}\n"
        "stdout:\n${CLANG_FORMAT_STDOUT}\n"
        "stderr:\n${CLANG_FORMAT_STDERR}")
endif ()
