# Zox Makefile

CC      = gcc
SRCS    = main.c \
          ast/ast_nodes.c ast/ast_free.c ast/ast_serial.c \
          lexer.c \
          parser/parser_core.c parser/parser_exprs.c parser/parser_stmts.c \
          values.c \
          eval/eval_core.c eval/eval_ops.c eval/eval_control.c \
          eval/eval_collections.c eval/eval_funcs.c eval/eval_import.c \
          malloc_safe.c env.c debug.c hash.c builtins.c global.c native_modules.c
LIBS    = -lm -ldl -Wl,--export-dynamic
BIN     = zox

CFLAGS_RELEASE = -O2
CFLAGS_DEV     = -g -fsanitize=address -fno-omit-frame-pointer
CFLAGS_LIB     = -shared -fPIC -O2

# Descobre automaticamente todos os .c em lib/
LIB_SRCS := $(wildcard lib/*.c)
LIB_SOS  := $(LIB_SRCS:.c=.so)

.PHONY: all dev test libs buildlib full fulldev testlib testlibs clean install listlibs help

## Build release do core (padrão)
all:
	$(CC) $(CFLAGS_RELEASE) -o $(BIN) $(SRCS) $(LIBS)

## Build do core com AddressSanitizer
dev:
	$(CC) $(CFLAGS_DEV) -o $(BIN) $(SRCS) $(LIBS)

## Compila todas as libs em lib/*.c -> lib/*.so
libs: $(LIB_SOS)

lib/%.so: lib/%.c
	$(CC) $(CFLAGS_LIB) -o $@ $< -I.

## Compila lib especifica:  make buildlib LIB=json
buildlib:
	$(CC) $(CFLAGS_LIB) -o lib/$(LIB).so lib/$(LIB).c -I.

## Build completo: core + todas as libs
full: all libs

## Build de dev completo: core dev + todas as libs
fulldev: dev libs

## Compila (release) e roda a suite de testes
test: all
	./$(BIN) tests/run_tests.zo


## Testa uma lib especifica:  make testlib LIB=json
testlib: lib/$(LIB).so
	@./$(BIN) tests/libs/$(LIB).zo > /tmp/zox_got_$(LIB).txt 2>&1; 	if diff -q tests/libs/$(LIB).expected /tmp/zox_got_$(LIB).txt > /dev/null 2>&1; then 		echo "  PASS  $(LIB)"; 	else 		echo "  FAIL  $(LIB)"; 		diff tests/libs/$(LIB).expected /tmp/zox_got_$(LIB).txt; 	fi

## Testa todas as libs em tests/libs/
testlibs: all libs
	@echo "=========================================="
	@echo "  Zox Lib Test Suite"
	@echo "=========================================="
	@pass=0; fail=0; 	for zo in tests/libs/*.zo; do 		name=$$(basename $$zo .zo); 		expected=tests/libs/$$name.expected; 		test -f "$$expected" || continue; 		./$(BIN) $$zo > /tmp/zox_got_$$name.txt 2>&1; 		if diff -q $$expected /tmp/zox_got_$$name.txt > /dev/null 2>&1; then 			echo "  PASS  $$name"; pass=$$((pass+1)); 		else 			echo "  FAIL  $$name"; fail=$$((fail+1)); 			diff $$expected /tmp/zox_got_$$name.txt; 		fi; 	done; 	echo "------------------------------------------"; 	echo "  Resultado: $$pass passou(ram) / $$fail falhou(ram)"; 	echo "------------------------------------------"

## Remove binario e libs compiladas
clean:
	rm -f $(BIN) $(LIB_SOS)

## Instala em /usr/local/bin
install: all
	cp $(BIN) /usr/local/bin/$(BIN)

## Lista libs disponíveis
listlibs:
	@echo "Libs disponíveis em lib/:"
	@for f in $(LIB_SRCS); do echo "  $$(basename $$f .c)"; done

## Ajuda
help:
	@echo "Targets disponíveis:"
	@echo "  make          — compila o core (release)"
	@echo "  make dev      — compila o core com AddressSanitizer"
	@echo "  make libs     — compila todas as libs em lib/*.c"
	@echo "  make buildlib LIB=json  — compila lib/json.so"
	@echo "  make full     — core + todas as libs"
	@echo "  make fulldev  — core dev + todas as libs"
	@echo "  make test     — compila e roda a suite de testes"
	@echo "  make listlibs — lista libs disponíveis"
	@echo "  make clean    — remove binário e .so compilados"
	@echo "  make install  — instala em /usr/local/bin"
