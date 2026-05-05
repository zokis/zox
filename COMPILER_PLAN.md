# Plano de Implementação: Compilador Zox x64

Este documento descreve as etapas para converter o interpretador Zox em um compilador AOT (Ahead-Of-Time) para arquitetura x64.

## Fase 1: Infraestrutura e Ferramentas
- [ ] **Seleção de Assembler:** Decidir entre NASM (sintaxe Intel) ou GAS (sintaxe AT&T). *Recomendado: NASM pela clareza.*
- [ ] **Integração no Build:** Atualizar o `Makefile` ou `CMakeLists.txt` para suportar a compilação de arquivos `.s`.
- [ ] **Runtime Linking:** Garantir que o `values.o`, `eval_ops.o` e outros módulos de runtime possam ser linkados estaticamente com o código gerado.
- [ ] **Flag de CLI:** Implementar a flag `--emit-asm` e `-o` no `main.c`.

## Fase 2: Geração de Código Base (Expressões)
- [ ] **Manejo da Pilha:** Implementar o uso da pilha x64 (`push`/`pop`) para gerenciar resultados intermediários da AST.
- [ ] **Literais Numéricos:** Gerar código que chama `MK_NUMBER` para cada constante.
- [ ] **Operações Binárias:** Implementar a chamada para `eval_binary_expr_evaluated`.
    - [ ] Mapear registradores para a ABI System V (RDI, RSI, RDX para argumentos).
- [ ] **Literais de String/Booleano:** Implementar chamadas para `MK_STRING` e `MK_BOOL`.

## Fase 3: Variáveis e Escopo
- [ ] **Resolução de Símbolos:** Implementar chamadas para `env_get` e `env_set` (ou similares) no runtime.
- [ ] **Identificadores:** Traduzir `IdentifierAst` para uma busca no `Environment` global/local.
- [ ] **Declarações e Atribuições:** Gerar chamadas para `declare_var` e `assign_var`.

## Fase 4: Fluxo de Controle
- [ ] **Labels Únicos:** Implementar um gerador de nomes de labels únicos (ex: `L_if_start_1`, `L_if_else_1`).
- [ ] **Condicionais (If):**
    - [ ] Avaliar condição -> Verificar se é `true` -> Pular para o label correspondente.
- [ ] **Loops (While/For):**
    - [ ] Implementar labels de início e fim.
    - [ ] Suporte para `break` e `continue` (usando labels pré-definidos no contexto da geração).

## Fase 5: Funções e Chamadas
- [ ] **Definição de Funções:** Traduzir `FuncDefAst` para um bloco de código com prologo e epilogo.
- [ ] **Chamadas de Função:** Implementar `CallExprAst` respeitando a convenção de chamada do runtime do Zox.
- [ ] **Closures:** Adaptar a captura de ambiente para funcionar com código compilado.

## Fase 6: Otimização e Estabilização
- [ ] **Peephole Optimization:** Evitar `push`/`pop` redundantes em operações simples.
- [ ] **Inlining de Tipos Básicos:** Tentar operar diretamente com `double` em operações aritméticas puras (quando o tipo puder ser inferido).
- [ ] **Suporte a Módulos Nativos:** Garantir que `import` funcione corretamente chamando os inicializadores do runtime.

---

### Exemplo de Fluxo de Trabalho
```bash
# 1. Gerar Assembly
./zox --emit-asm script.zo -o script.s

# 2. Montar
nasm -f elf64 script.s -o script.o

# 3. Linkar com o Runtime
gcc script.o values.o eval_ops.o env.o zox_alloc.o -o script.exe
```
