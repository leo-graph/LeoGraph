option(ENABLE_CLANG_FORMAT "Enable `clang-format` integration targets" ON)
option(ENABLE_CLANG_FORMAT_ON_BUILD "Run `clang-format-all` as part of the default build" OFF)

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

set(CLANG_FORMAT_RUNNER "${ClickHouse_SOURCE_DIR}/cmake/run_clang_format.cmake")
set(CLANG_FORMAT_TARGET_ARGS
    -DCLANG_FORMAT_EXECUTABLE=${CLANG_FORMAT_EXECUTABLE}
    -DCLICKHOUSE_SOURCE_DIR=${ClickHouse_SOURCE_DIR}
    -DCLANG_FORMAT_BINARY_DIR=${CMAKE_BINARY_DIR})

if (ENABLE_CLANG_FORMAT_ON_BUILD)
    add_custom_target(
        clang-format-all ALL
        COMMAND ${CMAKE_COMMAND} ${CLANG_FORMAT_TARGET_ARGS} -DCLANG_FORMAT_MODE=format -P ${CLANG_FORMAT_RUNNER}
        COMMENT "Formatting tracked ClickHouse sources with `clang-format`"
        VERBATIM)
else ()
    add_custom_target(
        clang-format-all
        COMMAND ${CMAKE_COMMAND} ${CLANG_FORMAT_TARGET_ARGS} -DCLANG_FORMAT_MODE=format -P ${CLANG_FORMAT_RUNNER}
        COMMENT "Formatting tracked ClickHouse sources with `clang-format`"
        VERBATIM)
endif ()

add_custom_target(
    check-clang-format
    COMMAND ${CMAKE_COMMAND} ${CLANG_FORMAT_TARGET_ARGS} -DCLANG_FORMAT_MODE=check -P ${CLANG_FORMAT_RUNNER}
    COMMENT "Checking tracked ClickHouse sources with `clang-format`"
    VERBATIM)
