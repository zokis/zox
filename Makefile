# Zox Makefile

CC      = gcc
SRCS    = main.c \
          ast/ast_nodes.c ast/ast_free.c ast/ast_serial.c \
          lexer.c \
          parser/parser_core.c parser/parser_exprs.c parser/parser_stmts.c \
          values.c \
          eval/eval_core.c eval/eval_ops.c eval/eval_control.c \
          eval/eval_collections.c eval/eval_funcs.c eval/eval_import.c \
          malloc_safe.c zox_alloc.c env.c debug.c hash.c builtins.c global.c native_modules.c
LIBS    = -lm -ldl -Wl,--export-dynamic
BIN     = zox

CFLAGS_RELEASE = -O2
CFLAGS_DEV     = -g -fsanitize=address -fno-omit-frame-pointer
CFLAGS_LIB     = -shared -fPIC -O2

# Auto-discover lib/*.c files.
LIB_SRCS := $(wildcard lib/*.c)
LIB_SOS  := $(LIB_SRCS:.c=.so)

.PHONY: all dev test libs buildlib full fulldev testlib testlibs perf perf-update bench-leak clean install listlibs help

## Build release core (default).
all:
	$(CC) $(CFLAGS_RELEASE) -o $(BIN) $(SRCS) $(LIBS)

## Build core with AddressSanitizer.
dev:
	$(CC) $(CFLAGS_DEV) -o $(BIN) $(SRCS) $(LIBS)

## Build all lib/*.c files into lib/*.so.
libs: $(LIB_SOS)

lib/%.so: lib/%.c
	$(CC) $(CFLAGS_LIB) -o $@ $< -I.

## Build specific lib: make buildlib LIB=json.
buildlib:
	$(CC) $(CFLAGS_LIB) -o lib/$(LIB).so lib/$(LIB).c -I.

## Build core and all libs.
full: all libs

## Build dev core and all libs.
fulldev: dev libs

## Build release and run test suite.
test: all
	./$(BIN) tests/run_unit_tests.zo


## Test specific lib: make testlib LIB=json.
testlib: all libs
	@./$(BIN) tests/libs/unit/$(LIB).zo

## Test all libs in tests/libs/unit/.
testlibs: all libs
	./$(BIN) tests/run_libs_unit_tests.zo

## Performance regression test (requires tests/perf_baseline.txt — run perf-update first).
perf:
	@bash scripts/perf_test.sh

## Save current performance as the new baseline.
perf-update:
	@bash scripts/perf_test.sh --update

## Run benchmarks in dev mode to detect memory leaks.
bench-leak: fulldev
	@echo "Checking leaks in bench.zo (Standard)..."
	@ASAN_OPTIONS=detect_leaks=1 ./$(BIN) tests/benchmarks/bench.zo > /dev/null
	@echo "Checking leaks in bench.zo (Arena 16MB)..."
	@ASAN_OPTIONS=detect_leaks=1 ./$(BIN) --arena=16 tests/benchmarks/bench.zo > /dev/null
	@echo "Checking leaks in bench_memory.zo (Standard)..."
	@ASAN_OPTIONS=detect_leaks=1 ./$(BIN) tests/benchmarks/bench_memory.zo > /dev/null
	@echo "Checking leaks in bench_memory.zo (Arena 16MB)..."
	@ASAN_OPTIONS=detect_leaks=1 ./$(BIN) --arena=16 tests/benchmarks/bench_memory.zo > /dev/null
	@echo "No leaks detected."

## Remove binary and compiled libs.
clean:
	rm -f $(BIN) $(LIB_SOS)

## Install into /usr/local/bin.
install: all
	cp $(BIN) /usr/local/bin/$(BIN)

## List available libs.
listlibs:
	@echo "Available libs in lib/:"
	@for f in $(LIB_SRCS); do echo "  $$(basename $$f .c)"; done

## Help.
help:
	@echo "Available targets:"
	@echo "  make          - build core (release)"
	@echo "  make dev      - build core with AddressSanitizer"
	@echo "  make libs     - build all libs in lib/*.c"
	@echo "  make buildlib LIB=json  - build lib/json.so"
	@echo "  make full     - core + all libs"
	@echo "  make fulldev  - dev core + all libs"
	@echo "  make test     - build and run test suite"
	@echo "  make testlibs - run all library unit tests"
	@echo "  make perf     - run performance regression tests"
	@echo "  make perf-update - update performance baseline"
	@echo "  make bench-leak - run benchmarks with leak detection"
	@echo "  make listlibs - list available libs"
	@echo "  make clean    - remove binary and compiled .so files"
	@echo "  make install  - install into /usr/local/bin"
