cmake_minimum_required(VERSION 3.20)
project(zox C)

set(CMAKE_C_STANDARD 11)

# ── Sources ────────────────────────────────────────────────────────────────────
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

# Inclui o diretório raiz para que os headers sejam encontrados
target_include_directories(zox PRIVATE ${CMAKE_SOURCE_DIR})

# ── Flags de compilação ────────────────────────────────────────────────────────
if (MSVC)
    target_compile_options(zox PRIVATE /W3)
else()
    target_compile_options(zox PRIVATE -Wall -Wextra)
endif()

# ── Bibliotecas por plataforma ─────────────────────────────────────────────────
if (WIN32)
    # No Windows: math.h já está embutida no MSVC/MinGW; sem suporte a .so/.dll dinâmico por ora
    message(STATUS "Build Windows: módulos dinâmicos (.dll) desabilitados")
else()
    # Linux / macOS: linka libm e libdl para módulos dinâmicos (.so)
    target_link_libraries(zox PRIVATE m dl)

    # Exporta todos os símbolos para que módulos .so possam usar funções do core
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(zox PRIVATE -Wl,--export-dynamic)
    endif()
endif()

# ── Perfil Debug vs Release ────────────────────────────────────────────────────
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    target_compile_options(zox PRIVATE -g -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(zox PRIVATE -fsanitize=address)
endif()
