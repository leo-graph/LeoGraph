option(ENABLE_CLANG_FORMAT "Enable `clang-format` integration targets" ON)
option(ENABLE_CLANG_FORMAT_ON_BUILD "Run target-scoped `clang-format` before building registered targets" ON)

if (NOT ENABLE_CLANG_FORMAT)
    return()
endif ()

find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if (CLANG_FORMAT_EXECUTABLE STREQUAL CLANG_FORMAT_EXECUTABLE-NOTFOUND)
    message(WARNING "`clang-format` not found, format targets are disabled")
    return()
endif ()

execute_process(
    COMMAND ${CLANG_FORMAT_EXECUTABLE} --version
    OUTPUT_VARIABLE CLANG_FORMAT_VERSION_STR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Using `clang-format`: ${CLANG_FORMAT_VERSION_STR}")

set(CLANG_FORMAT_RUNNER "${CMAKE_CURRENT_LIST_DIR}/run_clang_format.cmake")
set(CLANG_FORMAT_TARGET_ARGS
    -DCLANG_FORMAT_EXECUTABLE=${CLANG_FORMAT_EXECUTABLE}
    -DCLICKHOUSE_SOURCE_DIR=${ClickHouse_SOURCE_DIR}
    -DCLANG_FORMAT_BINARY_DIR=${CMAKE_BINARY_DIR})

add_custom_target(
    clang-format-all
    COMMAND ${CMAKE_COMMAND} ${CLANG_FORMAT_TARGET_ARGS} -DCLANG_FORMAT_MODE=format -P ${CLANG_FORMAT_RUNNER}
    COMMENT "Formatting tracked ClickHouse sources with `clang-format`"
    VERBATIM)

add_custom_target(
    check-clang-format
    COMMAND ${CMAKE_COMMAND} ${CLANG_FORMAT_TARGET_ARGS} -DCLANG_FORMAT_MODE=check -P ${CLANG_FORMAT_RUNNER}
    COMMENT "Checking tracked ClickHouse sources with `clang-format`"
    VERBATIM)

function(GET_TARGET_SOURCES TARGET RESULT)
    get_property(HAS_SOURCE_DIR TARGET ${TARGET} PROPERTY SOURCE_DIR SET)
    get_property(HAS_BINARY_DIR TARGET ${TARGET} PROPERTY BINARY_DIR SET)
    get_target_property(SOURCE_DIR ${TARGET} SOURCE_DIR)
    get_target_property(BINARY_DIR ${TARGET} BINARY_DIR)
    get_target_property(TARGET_SOURCE_LIST ${TARGET} SOURCES)

    unset(FILTERED_SOURCES)

    foreach(source_file IN LISTS TARGET_SOURCE_LIST)
        if (NOT source_file MATCHES ".*\\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc|ipp|tpp)$")
            continue()
        endif ()

        unset(RESOLVED_SOURCE)
        if (IS_ABSOLUTE "${source_file}")
            set(RESOLVED_SOURCE "${source_file}")
        elseif (HAS_SOURCE_DIR AND EXISTS "${SOURCE_DIR}/${source_file}")
            set(RESOLVED_SOURCE "${SOURCE_DIR}/${source_file}")
        elseif (HAS_BINARY_DIR AND EXISTS "${BINARY_DIR}/${source_file}")
            set(RESOLVED_SOURCE "${BINARY_DIR}/${source_file}")
        endif ()

        if (NOT DEFINED RESOLVED_SOURCE)
            continue()
        endif ()

        cmake_path(NORMAL_PATH RESOLVED_SOURCE OUTPUT_VARIABLE NORMALIZED_SOURCE)

        if (NOT NORMALIZED_SOURCE MATCHES "^${ClickHouse_SOURCE_DIR}/")
            continue()
        endif ()

        if (NORMALIZED_SOURCE MATCHES "^${ClickHouse_SOURCE_DIR}/contrib/")
            continue()
        endif ()

        if (NORMALIZED_SOURCE MATCHES "^${CMAKE_BINARY_DIR}/")
            continue()
        endif ()

        list(APPEND FILTERED_SOURCES "${NORMALIZED_SOURCE}")
    endforeach ()

    if (FILTERED_SOURCES)
        list(REMOVE_DUPLICATES FILTERED_SOURCES)
    endif ()

    set(${RESULT} "${FILTERED_SOURCES}" PARENT_SCOPE)
endfunction()

function(CLANG_FORMAT_SETUP TARGET)
    if (NOT ENABLE_CLANG_FORMAT)
        return()
    endif ()

    if (NOT TARGET ${TARGET})
        return()
    endif ()

    GET_TARGET_SOURCES(${TARGET} TARGET_SOURCES)
    if (NOT TARGET_SOURCES)
        return()
    endif ()

    set(FORMAT_TARGET_NAME ${TARGET}_clang_format)
    set(FORMAT_FILE_LIST "${CMAKE_CURRENT_BINARY_DIR}/${FORMAT_TARGET_NAME}_files.txt")
    set(FORMAT_STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${FORMAT_TARGET_NAME}.stamp")
    string(REPLACE ";" "\n" FORMAT_FILE_LIST_CONTENT "${TARGET_SOURCES}")
    file(GENERATE OUTPUT "${FORMAT_FILE_LIST}" CONTENT "${FORMAT_FILE_LIST_CONTENT}\n")
    set(FORMAT_COMMAND_ARGS
        ${CLANG_FORMAT_TARGET_ARGS}
        -DCLANG_FORMAT_MODE=format
        -DCLANG_FORMAT_INPUT_FILE=${FORMAT_FILE_LIST}
        -DCLANG_FORMAT_REFERENCE_FILE=${FORMAT_STAMP_FILE}
        -DCLANG_FORMAT_STYLE_FILE=${ClickHouse_SOURCE_DIR}/.clang-format
        -DCLANG_FORMAT_RUNNER_FILE=${CLANG_FORMAT_RUNNER}
        -DCLANG_FORMAT_MANIFEST_FILE=${FORMAT_FILE_LIST})
    set(FORMAT_DEPENDS
        ${TARGET_SOURCES}
        "${FORMAT_FILE_LIST}"
        "${CLANG_FORMAT_RUNNER}"
        "${ClickHouse_SOURCE_DIR}/.clang-format")

    if (EXISTS "${ClickHouse_SOURCE_DIR}/.clang-format-ignore")
        list(APPEND FORMAT_DEPENDS "${ClickHouse_SOURCE_DIR}/.clang-format-ignore")
        list(APPEND FORMAT_COMMAND_ARGS -DCLANG_FORMAT_IGNORE_FILE=${ClickHouse_SOURCE_DIR}/.clang-format-ignore)
    endif ()

    add_custom_command(
        OUTPUT "${FORMAT_STAMP_FILE}"
        COMMAND ${CMAKE_COMMAND}
            ${FORMAT_COMMAND_ARGS}
            -P ${CLANG_FORMAT_RUNNER}
        COMMAND ${CMAKE_COMMAND} -E touch "${FORMAT_STAMP_FILE}"
        DEPENDS ${FORMAT_DEPENDS}
        COMMENT "Formatting target ${TARGET} sources with `clang-format`"
        VERBATIM)

    add_custom_target(
        ${FORMAT_TARGET_NAME}
        DEPENDS "${FORMAT_STAMP_FILE}")

    if (ENABLE_CLANG_FORMAT_ON_BUILD)
        add_dependencies(${TARGET} ${FORMAT_TARGET_NAME})
    endif ()
endfunction()
