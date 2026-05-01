# Zox — Especificação da Linguagem

## Visão Geral

Zox é uma linguagem **interpretada**, **expression-based** e **dinamicamente tipada**. Todo bloco de código retorna um valor (o último valor avaliado). A sintaxe é minimalista, usando símbolos especiais em vez de palavras-chave longas.

---

## Tipos de Dado

| Tipo       | Descrição                              | Exemplo                         |
|------------|----------------------------------------|---------------------------------|
| `nil`      | Ausência de valor                      | `nil`                           |
| `boolean`  | Verdadeiro ou falso                    | `true`, `false`                 |
| `number`   | Ponto flutuante de 64 bits             | `42`, `3.14`, `-7`              |
| `string`   | Sequência de caracteres imutável       | `"hello"`, `'world'`            |
| `list`     | Array dinâmico heterogêneo             | `{1, "two", true}`              |
| `dict`     | Mapa chave→valor                       | `["x" -> 1; "y" -> 2]`          |
| `function` | Valor de função (first-class)          | `$ f(x) { x * 2 }`              |

---

## Literais

### Números
```zox
42
3.14
-100
```

### Strings
Aspas simples ou duplas são equivalentes.
```zox
"Olá, mundo!"
'Zox lang'
```

### Booleanos e Nil
```zox
true
false
nil
```

### Listas
Delimitadas por `{ }`, elementos separados por vírgula.
```zox
let nums = {1, 2, 3};
let mixed = {"a", 42, true, nil};
let empty = {};
```

### Dicionários
Delimitados por `[ ]`, pares separados por `;`, usando `->` entre chave e valor.
```zox
let person = ["name" -> "Alice"; "age" -> 30];
let empty_dict = [];
```

---

## Constantes Globais

| Constante | Valor              |
|-----------|--------------------|
| `nil`     | Valor nulo         |
| `true`    | Booleano verdadeiro|
| `false`   | Booleano falso     |
| `PI`      | 3.14159265359      |

---

## Variáveis

### Declaração
```zox
let x = 10;
let name = "Zox";
let flag = true;
```

### Atribuição
```zox
x = 20;
```

### Atribuição em coleções
```zox
let arr = {1, 2, 3};
arr[0] = 99;            -# modifica o primeiro elemento

let d = ["k" -> "v"];
d{"k"} = "novo";        -# modifica valor no dict
```

---

## Comentários

Apenas comentários de linha, iniciados com `-#`.
```zox
let x = 5; -# isto é um comentário
-# linha inteira comentada
```

---

## Operadores

### Aritméticos
| Operador | Descrição         | Exemplo          |
|----------|-------------------|------------------|
| `+`      | Adição            | `3 + 2` → `5`    |
| `-`      | Subtração         | `5 - 3` → `2`    |
| `*`      | Multiplicação     | `4 * 3` → `12`   |
| `/`      | Divisão           | `10 / 4` → `2.5` |
| `%`      | Módulo            | `7 % 3` → `1`    |
| `**`     | Potência          | `2 ** 8` → `256` |

### Comparação
| Operador | Descrição          |
|----------|--------------------|
| `==`     | Igual              |
| `!=`     | Diferente          |
| `>`      | Maior              |
| `<`      | Menor              |
| `>=`     | Maior ou igual     |
| `<=`     | Menor ou igual     |

### Lógicos
| Operador | Descrição  |
|----------|------------|
| `&&`     | AND lógico |
| `\|\|`   | OR lógico  |

### Bitwise
| Operador | Descrição         |
|----------|-------------------|
| `&`      | AND bitwise       |
| `\|`     | OR bitwise        |
| `^`      | XOR bitwise       |
| `<<`     | Shift left        |
| `>>`     | Shift right       |

### Operadores de String
| Operador | Descrição                              | Exemplo                        |
|----------|----------------------------------------|--------------------------------|
| `+`      | Concatenação                           | `"foo" + "bar"` → `"foobar"`  |
| `-`      | Remove todas as ocorrências            | `"abba" - "b"` → `"aa"`      |
| `*`      | Repetição                              | `"ab" * 3` → `"ababab"`      |
| `==`     | Igualdade                              | `"a" == "a"` → `true`        |
| `!=`     | Diferença                              | `"a" != "b"` → `true`        |

### Operadores de Lista
| Operador | Descrição                     | Exemplo                              |
|----------|-------------------------------|--------------------------------------|
| `+`      | Concatenação                  | `{1,2} + {3,4}` → `{1,2,3,4}`      |
| `-`      | Diferença de conjuntos        | `{1,2,3} - {2}` → `{1,3}`          |
| `*`      | Produto cartesiano / repetição| `{1,2} * 2` → `{1,2,1,2}`          |
| `&`      | Interseção                    | `{1,2,3} & {2,3,4}` → `{2,3}`      |
| `\|`     | União (sem duplicatas)        | `{1,2} \| {2,3}` → `{1,2,3}`       |
| `^`      | Diferença simétrica           | `{1,2,3} ^ {2,3,4}` → `{1,4}`      |
| `<<`     | Append (muta a lista)         | `lista << 10`                        |

### Operadores element-wise em listas
Prefixo `&` aplicado elemento a elemento com um escalar:
`&+`, `&-`, `&*`, `&/`, `&%`

```zox
let a = {1, 2, 3};
println(a &+ 10);  -# {11, 12, 13}
println(a &* 2);   -# {2, 4, 6}
```

### Operadores de Dicionário
| Operador | Descrição                           |
|----------|-------------------------------------|
| `+`      | Merge (direita sobrescreve esquerda)|

---

## Precedência de Operadores

Do menor para o maior (operadores de menor precedência são avaliados por último):

| Nível | Operadores                      |
|-------|---------------------------------|
| 1     | Atribuição `=`                  |
| 2     | OR lógico `\|\|`                |
| 3     | AND lógico `&&`                 |
| 4     | OR bitwise `\|`                 |
| 5     | XOR bitwise `^`                 |
| 6     | AND bitwise `&`                 |
| 7     | Igualdade `==`, `!=`            |
| 8     | Comparação `<`, `<=`, `>`, `>=` |
| 9     | Shifts `<<`, `>>`               |
| 10    | Aditivos `+`, `-`               |
| 11    | Multiplicativos `*`, `/`, `%`   |
| 12    | Unário `-`                      |
| 13    | Primários (literals, calls…)    |

---

## Controle de Fluxo

### Condicional — `?`

```zox
? (condição) {
    bloco_verdadeiro
} :? (outra_condição) {
    bloco_else_if
} : {
    bloco_else
}
```

Exemplo:
```zox
let x = 15;
? (x > 20) {
    println("grande")
} :? (x > 10) {
    println("médio")
} : {
    println("pequeno")
}
```

O condicional é uma expressão e retorna o último valor avaliado no bloco executado.

### Loop while — `#`

```zox
#(condição) {
    corpo
}
```

Exemplo:
```zox
let i = 0;
#(i < 5) {
    println(i);
    i = i + 1
}
```

### Loop for — `@`

```zox
@(inicialização; condição; incremento) {
    corpo
}
```

Exemplo:
```zox
@(let i = 0; i < 10; i = i + 1) {
    println(i)
}
```

### Break — `~!!`

Interrompe o loop atual imediatamente.

```zox
let i = 0;
#(i < 100) {
    ?(i == 5) {
        ~!!
    }
    i = i + 1
}
println(i)  -# 5
```

### Continue — `__>`

Pula para a próxima iteração do loop.

```zox
@(let i = 0; i < 10; i = i + 1) {
    ?(i % 2 == 0) {
        __>
    }
    println(i)  -# imprime só os ímpares: 1 3 5 7 9
}
```

### Return — `_>>`

Retorna um valor de uma função antes do final do corpo. Pode ser usado para retorno antecipado.

```zox
$ primeiro_negativo(lista) {
    let i = 0;
    #(i < len(lista)) {
        ?(lista[i] < 0) {
            _>> lista[i]
        }
        i = i + 1
    }
    _>> nil
}

println(primeiro_negativo({1, 2, -3, 4}));  -# -3
println(primeiro_negativo({1, 2, 3}));       -# nil
```

`_>>` sem valor retorna `nil`:
```zox
$ valida(x) {
    ?(x < 0) {
        _>>
    }
    println("ok")
}
```

---

## Funções

### Definição
Usando `$`. O valor retornado é o resultado da última expressão avaliada no corpo, ou o valor de um `_>>` explícito.

```zox
$ nome(param1, param2) {
    corpo
}
```

Exemplos:
```zox
$ soma(a, b) {
    a + b
}

$ fatorial(n) {
    ?(n <= 1) {
        _>> 1
    }
    n * fatorial(n - 1)
}
```

### Chamada
```zox
soma(3, 4)          -# 7
fatorial(5)         -# 120
println("oi")       -# chama builtin
```

### Funções como valores (first-class)
```zox
$ dobro(x) { x * 2 };
let f = dobro;
println(f(5))  -# 10
```

### Closures
Funções capturam o escopo onde foram definidas:
```zox
$ make_adder(n) {
    $ adder(x) { x + n }
}
let add5 = make_adder(5);
println(add5(10))  -# 15
```

---

## Acesso a Coleções

### Listas — índice e slice
```zox
let a = {10, 20, 30, 40};
println(a[0]);      -# 10
println(a[-1]);     -# 40
println(a[1:3]);    -# {20, 30}
println(a[:2]);     -# {10, 20}
println(a[2:]);     -# {30, 40}
```

### Dicionários
```zox
let d = ["x" -> 100; "y" -> 200];
println(d{"x"});    -# 100
d{"z"} = 300;       -# insere nova chave
```

---

## Módulos e Imports

### Sintaxe de import
```zox
~> modulo {funcao1, funcao2 as alias};
```

Exemplos:
```zox
~> math {abs, sqrt, sin, cos};
~> math {sqrt as raiz};
~> file {open, fRead, fWrite, fClose};
~> assert {assert_eq, assert_true, assert_summary};
```

Módulos nativos (`math`, `file`) são embutidos no interpretador.
Módulos Zox puro são buscados em `./`, `./lib/` e `./packages/`.

### Módulo `math`

| Função           | Argumentos          | Retorno | Descrição                     |
|------------------|---------------------|---------|-------------------------------|
| `abs(x)`         | number              | number  | Valor absoluto                |
| `sqrt(x)`        | number              | number  | Raiz quadrada                 |
| `sin(x)`         | number              | number  | Seno (radianos)               |
| `cos(x)`         | number              | number  | Cosseno (radianos)            |
| `tan(x)`         | number              | number  | Tangente (radianos)           |
| `log(x)`         | number              | number  | Logaritmo natural             |
| `floor(x)`       | number              | number  | Arredonda para baixo          |
| `ceil(x)`        | number              | number  | Arredonda para cima           |
| `round(x)`       | number              | number  | Arredonda para o mais próximo |
| `min(a, b)`      | number, number      | number  | Mínimo entre dois números     |
| `max(a, b)`      | number, number      | number  | Máximo entre dois números     |
| `pow(x, y)`      | number, number      | number  | Potência: x^y                 |
| `lmin(lista)`    | list                | number  | Mínimo dos elementos          |
| `lmax(lista)`    | list                | number  | Máximo dos elementos          |
| `average(lista)` | list                | number  | Média aritmética              |
| `median(lista)`  | list                | number  | Mediana                       |

### Módulo `file`

| Função                  | Argumentos     | Retorno | Descrição                         |
|-------------------------|----------------|---------|-----------------------------------|
| `open(path, mode)`      | string, string | file    | Abre arquivo ("r", "w", "a", ...) |
| `fRead(file)`           | file           | string  | Lê o arquivo inteiro              |
| `fReadLine(file)`       | file           | string  | Lê uma linha (-1 no EOF)          |
| `fWrite(file, content)` | file, string   | nil     | Escreve no arquivo                |
| `fSeek(file, pos)`      | file, number   | nil     | Move ponteiro do arquivo          |
| `fClose(file)`          | file           | nil     | Fecha o arquivo                   |
| `fExists(path)`         | string         | boolean | Verifica se arquivo existe        |
| `fDelete(path)`         | string         | boolean | Deleta um arquivo                 |
| `fCopy(src, dst)`       | string, string | boolean | Copia arquivo                     |
| `fMove(src, dst)`       | string, string | boolean | Move/renomeia arquivo             |

### Módulo `assert` (`lib/assert.zo`)

Módulo escrito em Zox puro para testes. Mantém contadores de pass/fail.

| Função                       | Descrição                                      |
|------------------------------|------------------------------------------------|
| `assert_true(v)`             | v deve ser true                                |
| `assert_false(v)`            | v deve ser false                               |
| `assert_eq(a, b)`            | a deve ser igual a b                           |
| `assert_neq(a, b)`           | a deve ser diferente de b                      |
| `assert_lt(a, b)`            | a < b                                          |
| `assert_gt(a, b)`            | a > b                                          |
| `assert_lte(a, b)`           | a <= b                                         |
| `assert_gte(a, b)`           | a >= b                                         |
| `assert_in(elem, coll)`      | elem deve estar na coleção/string              |
| `assert_not_in(elem, coll)`  | elem não deve estar na coleção/string          |
| `assert_len(coll, n)`        | coleção deve ter tamanho n                     |
| `assert_contains(str, sub)`  | string deve conter sub                         |
| `assert_summary()`           | imprime "N passed / M failed"                  |

```zox
~> assert {assert_eq, assert_true, assert_summary};

assert_eq(1 + 1, 2);
assert_true(5 > 3);
assert_summary();   -# 2 passed / 0 failed
```

---

## Funções Built-in

Disponíveis sem import em qualquer programa Zox.

| Função               | Argumentos            | Retorno | Descrição                                  |
|----------------------|-----------------------|---------|--------------------------------------------|
| `print(val)`         | qualquer              | nil     | Imprime sem quebra de linha                |
| `println(val)`       | qualquer              | nil     | Imprime com quebra de linha                |
| `len(col)`           | string/list/dict      | number  | Tamanho da coleção                         |
| `keys(dict)`         | dict                  | list    | Lista de chaves do dicionário              |
| `values(dict)`       | dict                  | list    | Lista de valores do dicionário             |
| `sum(list)`          | list                  | number  | Soma dos elementos numéricos               |
| `find(target, val)`  | string/list/dict, any | number  | Índice do valor (-1 se não encontrado)     |
| `random()`           | nenhum                | number  | Número aleatório em [0, 1)                 |
| `randomInt(min,max)` | number, number        | number  | Inteiro aleatório em [min, max]            |

---

## Coerção de Tipos

- Booleanos podem ser usados em contextos numéricos: `true` → `1`, `false` → `0`.
- Não há coerção automática entre string e number.

---

## Exemplos Completos

### Hello World
```zox
println("Hello, World!");
```

### Fibonacci recursivo
```zox
$ fib(n) {
    ?(n <= 1) { n } : { fib(n-1) + fib(n-2) }
}

@(let i = 0; i < 10; i = i + 1) {
    println(fib(i))
}
```

### Fatorial com return explícito
```zox
$ fatorial(n) {
    ?(n <= 1) {
        _>> 1
    }
    n * fatorial(n - 1)
}
println(fatorial(10))  -# 3628800
```

### Break e continue
```zox
-# soma os primeiros 5 múltiplos de 3
let soma = 0;
let count = 0;
@(let i = 1; i < 1000; i = i + 1) {
    ?(i % 3 != 0) {
        __>
    }
    soma = soma + i;
    count = count + 1;
    ?(count == 5) {
        ~!!
    }
}
println(soma)  -# 45  (3+6+9+12+15)
```

### Operações com listas
```zox
let a = {1, 2, 3};
let b = {3, 4, 5};

println(a + b);    -# {1, 2, 3, 3, 4, 5}
println(a & b);    -# {3}
println(a | b);    -# {1, 2, 3, 4, 5}
println(a ^ b);    -# {1, 2, 4, 5}

a << 10;
println(a)         -# {1, 2, 3, 10}
```

### Dicionário e módulo math
```zox
~> math {sqrt, abs};

let data = ["val" -> -9; "label" -> "raiz"];
let v = abs(data{"val"});
println(data{"label"} + ": " + sqrt(v));  -# raiz: 3
```

### Leitura de arquivo
```zox
~> file {open, fReadLine, fClose};

let f = open("dados.txt", "r");
let linha = fReadLine(f);
#(linha != -1) {
    println(linha);
    linha = fReadLine(f)
}
fClose(f);
```
