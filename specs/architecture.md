# Zox — Arquitetura Geral

## Visão Geral

Zox é uma linguagem interpretada implementada em C, seguindo o pipeline clássico de um interpretador: **Lexer → Parser → AST → Evaluator**. O interpretador percorre a árvore de forma recursiva (_tree-walking interpreter_), sem geração de bytecode ou compilação intermediária.

---

## Pipeline de Execução

```
Código-fonte (.zox)
        │
        ▼
   ┌─────────┐
   │  LEXER  │  Tokenização — converte caracteres em tokens
   └────┬────┘
        │  Token[]
        ▼
   ┌─────────┐
   │ PARSER  │  Parsing recursivo descendente → constrói a AST
   └────┬────┘
        │  Program (AST)
        ▼
   ┌──────────────┐
   │  EVALUATOR   │  Interpreta a AST nó por nó
   └──────┬───────┘
          │  usa
    ┌─────┴──────┐
    │            │
    ▼            ▼
┌───────┐  ┌──────────────┐
│  ENV  │  │ RuntimeVal   │
│(scope)│  │ (tipos/vals) │
└───────┘  └──────────────┘
```

---

## Componentes

### `lexer.c / lexer.h`
Responsável pela **análise léxica**. Lê o código-fonte caractere por caractere e produz um array de `Token`.

- Reconhece: números, strings (aspas simples e duplas), identificadores, palavras-chave, operadores e símbolos especiais.
- Suporte a UTF-8 via `isalpha_custom()`.
- Comentários de linha iniciados com `-#`.
- Identificadores podem conter letras, dígitos e `_`.
- Palavras-chave especiais: `~>` (import), `~!!` (break), `__>` (continue), `_>>` (return).
- Verifica boundary no token `as` para não consumir identificadores como `assert_eq`.
- Retorna um array de `Token` com tipo (`TokenType`), valor textual e posição (linha/coluna).

### `parser.c / parser.h`
Responsável pelo **parsing** e construção da AST.

- Implementa um **parser recursivo descendente**.
- Ponto de entrada: `produce_ast()` → retorna um nó `Program`.
- Mantém estado em uma struct `Parser` com o array de tokens e o índice atual.
- Cadeia de precedência de operadores (do menor para o maior):
  ```
  assignment → logical_or → logical_and → equality →
  comparison → shift → additive → multiplicative → unary → primary
  ```
- `parse_stmt` reconhece `~!!`, `__>`, `_>>` e os consome (incluindo `;` opcional).

### `ast.c / ast.h`
Define as **estruturas de nós da AST** (Abstract Syntax Tree).

- 25 tipos de nós (`NodeType`).
- Dois supertypes: `Stmt` (declarações) e `Expr` (expressões).
- Pré-aloca literais comuns para eficiência de memória: `nil`, `true`, `false` e números 0–255.
- Inclui `BreakStmt`, `ContinueStmt`, `ReturnStmt` para controle de fluxo.

### `eval.c / eval.h`
O **avaliador** — coração do interpretador.

- Função principal: `evaluate(Stmt *node, Environment *env)`.
- Despacha no tipo do nó via `switch` e avalia recursivamente.
- Retorna sempre um `RuntimeVal *` com `ref_count` incrementado (`ref+1` para o chamador).
- **Mecanismo de ControlFlow**: variável global `cf_signal` (CF_NONE / CF_BREAK / CF_CONTINUE / CF_RETURN) propaga break/continue/return pelo call stack sem usar `longjmp`.
  - `eval_while_expr` e `eval_for_expr` checam `cf_signal` após cada statement e respondem localmente ou propagam.
  - `eval_call_expr` captura `CF_RETURN` e limpa o sinal, extraindo o valor com `cf_take_return_val()`.

### `values.c / values.h`
Define os **tipos de valor em tempo de execução** com **reference counting**.

- 7 tipos: `NIL_T`, `NUMBER_T`, `BOOLEAN_T`, `STRING_T`, `LIST_T`, `DICT_T`, `FUNCTION_T`.
- Cada tipo possui sua própria struct derivada de `RuntimeVal`.
- `nil`, `true`, `false` e números inteiros 0–255 são **singletons estáticos** com `ref_count = -1` — `retain`/`release` são no-op para singletons.
- Contrato uniforme: todo `evaluate()` retorna com `ref_count + 1`. O chamador é responsável pelo `release`.

### `env.c / env.h`
Gerencia os **escopos e a tabela de símbolos** com reference counting.

- Baseado em hash table (open addressing) com encadeamento de escopos pai.
- Funções: `declare_var()`, `declare_owned()`, `lookup_var()`, `assign_var()`, `resolve()`.
- `declare_owned(env, name, val)`: variante de `declare_var` que faz `retain` e depois `release` — útil para valores recém-criados (builtins, etc.).
- `retain_env` / `release_env`: reference counting no próprio `Environment`. Quando `ref_count` chega a zero, `destroy_environment` libera as entradas e propaga o `release` para o env pai.
- `owned_program`: ponteiro para a AST de módulos importados — mantém a AST viva enquanto `FunctionVal`s ainda apontam para os nós dela.
- `break_env_cycles(env)`: percorre o env e solta referências de funções que capturam o próprio env (evita ciclo no escopo global).
- Cada chamada de função e bloco de controle cria um escopo filho.

### `builtins.c / builtins.h`
Funções **sempre disponíveis** na linguagem, sem necessidade de import.

- Todas declaradas com `declare_owned` e `MK_FUNCTION(..., NULL, ...)` (sem captura de env — evita ciclo com o escopo global).

Veja a lista completa na [Especificação da Linguagem](./language-spec.md#funções-built-in).

### `native_modules.c / native_modules.h`
Sistema de **módulos nativos** importáveis via `~>`.

- Módulos disponíveis: `math`, `file`.
- Cada módulo é registrado em um array `NativeModule[]` com nome e função de inicialização.
- A função de inicialização injeta funções nativas no ambiente fornecido com `declare_owned`.

### `global.c / global.h`
Contexto de execução global (`ExecutionContext`).

- Flag `is_repl` — indica se está rodando no REPL.
- Buffer `error_jmp` (`setjmp`/`longjmp`) para tratamento de erros sem `exit()`.

### `hash.c / hash.h`
Utilitários de **hash** usados pelo `env` (open addressing) e pelo `DictVal` (chaining).

### `malloc_safe.c / malloc_safe.h`
Wrapper sobre `malloc`/`realloc`/`free` com verificação de erros. Aborta com mensagem descritiva em caso de falha de alocação.

### `debug.c`
Funções auxiliares para impressão de AST e tokens (fins educacionais e de depuração).

### `main.c`
Ponto de entrada. Inicializa o ambiente global, decide entre REPL e execução de arquivo, chama `break_env_cycles` antes de liberar o env global e aciona o pipeline.

---

## Estruturas de Dados Principais

### Token
```c
typedef struct {
    TokenType type;
    char     *value;
    int       line;
    int       column;
} Token;
```

### Parser
```c
typedef struct {
    Token         *tokens;
    long long int  token_count;
    long long int  current;
} Parser;
```

### Environment (escopo)
```c
struct Environment {
    Environment *parent;       // parent scope chain
    HashEntry   *entries;      // stored variables, open addressing
    size_t       capacity;
    size_t       size;
    char        *scope_name;   // debug scope name
    int          ref_count;    // reference counting
    void        *owned_program;// imported module AST owned by env
};
```

### RuntimeVal (base de todos os valores)
```c
typedef struct {
    ValueType type;
    int       ref_count;  // -1 = singleton, never freed
} RuntimeVal;

// Specializations:
NumberVal   { ValueType type; int ref_count; double value; }
BooleanVal  { ValueType type; int ref_count; int value; }
StringVal   { ValueType type; int ref_count; char *value; }
ListVal     { ValueType type; int ref_count; size_t size; size_t capacity; RuntimeVal **items; }
DictVal     { ValueType type; int ref_count; size_t size; size_t capacity; Entry **entries; }
FunctionVal { ValueType type; int ref_count;
              char **params; size_t param_count;
              Stmt **body;   size_t body_count;
              Environment *env;           // NULL para builtins
              RuntimeVal *(*builtin_func)(...); }
```

### ControlFlow (sinais internos do evaluator)
```c
typedef enum { CF_NONE = 0, CF_BREAK, CF_CONTINUE, CF_RETURN } ControlFlowKind;

static ControlFlowKind cf_signal   = CF_NONE;
static RuntimeVal     *cf_return_val = NULL;
```

### ExecutionContext
```c
typedef struct {
    int     is_repl;
    jmp_buf error_jmp;
} ExecutionContext;
```

---

## Gerenciamento de Memória

O interpretador usa **reference counting manual** para todos os `RuntimeVal` e `Environment`.

### Contrato de ownership
- Todo `evaluate()` retorna com `ref_count + 1` para o chamador.
- O chamador é responsável por chamar `release()` quando não precisar mais do valor.
- `retain(val)` incrementa `ref_count`; `release(val)` decrementa e libera se chegar a zero.
- `retain`/`release` são no-op para singletons (`ref_count == -1`).

### Casos especiais
| Situação | Como é tratado |
|----------|----------------|
| `declare_var(env, name, val)` | faz `retain` — env assume ownership |
| `declare_owned(env, name, val)` | `declare_var` + `release` — para valores recém-criados |
| `assign_var` | `release` no valor anterior, `retain` no novo |
| `list_append_val` / `dict_set_val` | fazem `retain` — coleção assume ownership |
| Funções capturando env | `MK_FUNCTION` faz `retain_env`; `free_runtime_val(FUNCTION_T)` faz `release_env` |
| Módulos importados | AST fica em `module_env->owned_program`, liberada quando o env é destruído |
| Ciclos globais | `break_env_cycles` solta referências func→env no escopo global antes de liberar |

### Singletons
`nil`, `true`, `false` e inteiros 0–255 são alocados estaticamente uma vez. Nunca são liberados.

---

## Fluxo de Execução — Passo a Passo

### Execução de arquivo
1. Lê o arquivo fonte como string.
2. Chama `tokenize(source, &token_count)` → array de `Token`.
3. Cria o parser: `create_parser(tokens, token_count)`.
4. Faz o parsing: `produce_ast(parser, source)` → nó `Program`.
5. Avalia: `eval_program(program, env)` → `RuntimeVal *`.
6. Libera memória: AST, tokens, parser, env (com `break_env_cycles` antes).

### REPL
1. Exibe o prompt `>>> `.
2. Lê uma linha do stdin.
3. Executa o mesmo pipeline (tokenize → parse → eval).
4. Se o resultado não for `nil`, imprime-o.
5. Retorna ao passo 1. Em caso de erro, `longjmp` retorna ao prompt sem encerrar o processo.

### Avaliação de expressão binária
```
evaluate(BinaryExprAst{ op: "+", left: 2, right: 3 })
  → lhs = evaluate(left)   → NumberVal(2)  [ref+1]
  → rhs = evaluate(right)  → NumberVal(3)  [ref+1]
  → result = eval_binary_expr_evaluated(lhs, rhs, "+") → NumberVal(5) [ref=1]
  → release(lhs)
  → release(rhs)
  → return result  [ref+1 para o chamador]
```

### Chamada de função (user-defined)
1. Avalia os argumentos (cada um com ref+1).
2. Cria um novo `Environment` filho do escopo de definição da função (closure).
3. `declare_var` de cada parâmetro no novo escopo (retain) + `release` do arg avaliado.
4. Avalia cada statement do corpo:
   - Checa `cf_signal` após cada statement.
   - Se `CF_RETURN`: extrai valor, limpa sinal, para o loop.
5. `retain(lastEvaluated)` antes de `free_environment` — preserva o valor de retorno.
6. Retorna `lastEvaluated` com ref+1 para o chamador.

### Propagação de break/continue/return
```
eval_while_expr
  └── for cada statement do corpo:
        evaluate(stmt)
        if cf_signal == CF_BREAK    → cf_signal = CF_NONE; break do loop
        if cf_signal == CF_CONTINUE → cf_signal = CF_NONE; próxima iteração
        if cf_signal == CF_RETURN   → propaga (não limpa)

eval_call_expr
  └── for cada statement do corpo:
        evaluate(stmt)
        if cf_signal == CF_RETURN:
          lastEvaluated = cf_take_return_val()
          cf_signal = CF_NONE
          break
```

---

## Tratamento de Erros

- Usa `setjmp`/`longjmp` para recuperação não-local.
- `error(message)` dispara `longjmp` para o handler registrado em `ExecutionContext`.
- **Modo arquivo**: aborta a execução com mensagem de erro.
- **REPL**: exibe o erro e retorna ao prompt, sem encerrar o processo.
