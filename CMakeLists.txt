cmake_minimum_required(VERSION 3.20)
project(zox C)

set(CMAKE_C_STANDARD 11)

# Sources.
set(SOURCES
    main.c
    ast/ast_nodes.c
    ast/ast_free.c
    ast/ast_serial.c
    lexer.c
    parser/parser_core.c
    parser/parser_exprs.c
    parser/parser_stmts.c
    values.c
    eval/eval_core.c
    eval/eval_ops.c
    eval/eval_control.c
    eval/eval_collections.c
    eval/eval_funcs.c
    eval/eval_import.c
    malloc_safe.c
    env.c
    debug.c
    hash.c
    builtins.c
    global.c
    native_modules.c
)

add_executable(zox ${SOURCES})

# Root include dir for project headers.
target_include_directories(zox PRIVATE ${CMAKE_SOURCE_DIR})

# Compiler flags.
if (MSVC)
    target_compile_options(zox PRIVATE /W3)
else()
    target_compile_options(zox PRIVATE -Wall -Wextra)
endif()

# Platform libraries.
if (WIN32)
    # Windows: math.h built in; dynamic .so/.dll modules disabled for now.
    message(STATUS "Windows build: dynamic modules (.dll) disabled")
else()
    # Linux/macOS: libm/libdl for dynamic modules.
    target_link_libraries(zox PRIVATE m dl)

    # Export core symbols for .so modules.
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(zox PRIVATE -Wl,--export-dynamic)
    endif()
endif()

# Debug vs release profile.
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    target_compile_options(zox PRIVATE -g -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(zox PRIVATE -fsanitize=address)
endif()
