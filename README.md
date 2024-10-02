
# Purpose
The primary goal of Zox is to provide a clear and simple implementation that illustrates key concepts in language design and implementation, such as:
- Lexical analysis (tokenization)
- Parsing and abstract syntax tree (AST) construction
- Symbol table management and scoping rules
- Interpretation and runtime behavior
- Memory management in interpreters

By studying and extending Zox, learners can gain insights into how more complex programming languages are implemented.

The Zox language is expression-based. All expressions return a value, and you don't need semicolons (;) at the end of expressions unless used for multiple statements in the same scope.

## Compiling Zox

To compile the Zox interpreter, use the following command in the terminal:

```bash
gcc -o zox main.c ast.c lexer.c parser.c values.c eval.c malloc_safe.c env.c debug.c hash.c builtins.c global.c native_modules.c -lm
```

## REPL (Read-Eval-Print Loop)

Zox includes an interactive REPL that allows users to experiment with the language dynamically. The REPL starts automatically when you run the Zox interpreter without arguments.

To start the REPL, simply run the Zox binary without arguments:

```bash
./zox
```

Once in the REPL, you can type Zox expressions or statements and see the results immediately. The REPL evaluates each input and prints the result.

Example REPL session:

```
Zox REPL
>>> $ square(n) { n * n }
<function>
>>> square(4)
16.000000
>>> let x = 5;
5.000000
>>> let y = 10;
10.000000
>>> x + y
15.000000
>>> let list = {x, y, 3, 4, 5};
{5.000000, 10.000000, 3.000000, 4.000000, 5.000000}
>>> list[2] + list[x - 2]
7.000000
>>> let dict = ["name" -> "Alice"; "age" -> 30];
["age" -> 30.000000; "name" -> "Alice"]
>>> dict{"name"}
Alice
>>> ? (dict{"age"} > 25) { "Adult" } : { "Young" }
Adult
>>>
```

## 1. Variables Declaration (let)
Variables are created using the let keyword. You can assign any value, including strings, numbers, booleans, lists, or dictionaries.
```
let name = "John";
let age = 20;
let isStudent = true;
```
## 2. Printing Values (print and println)
print: Outputs without a newline.
println: Outputs with a newline.
```
print("Name: ");
println(name);
```
## 3. Operators
Zox supports the following basic arithmetic operations:
- Addition (+)
- Subtraction (-)
- Multiplication (*)
- Division (/)
- Power (**)
- Modulus (%)
```
let sum = 10 + 5;
let sub = 20 - 0.1;
let mult = 33 * 2;
let div = 100 / 2;
let pot = 2 ** 3;
let mod = 100 % 3;
println(sum); -# 15
println(sub); -# 19.9
println(mult) -# 66
println(div) -# 50
println(pot) -# 8
println(mod) -# 1
```

## 4. Conditional Statements (if)

Zox uses `?` for if conditions. It allows chaining with `:?` for elif and `:` for else.
```
let age = 18;
let blond = false;
? (age < 18) {
    println("You are a minor")
} : {
    println("You are an adult")
}
? (blond && age >= 18) {
    println("You are blond and an adult")
} :? (blond && age < 18) {
    println("You are blond and a minor")
} :? (age >= 18 && blond == false) {
    println("You are not blond and an adult")
} : {
    println("You are not blond and a minor")
}
```

Note: These conditions are expressions and return the last evaluated value.

## 5. Loops
For Loops `@`: Similar to traditional for loops but in the form of expressions.
```
let a = 1;
let c = 100;
let b = @(let x = 0; x <= 2; x = x + 1) {
    let c = a * (x + 1);
    a = c + 1
};
println(b); -# 16
println(c); -# 100
println(a) -# 16
```
While Loops `#`: Used for looping based on a condition.
```
let a = 10;
let b = 0;
let c = #(a > 0) {
    b = b + 1;
    a = a - 1;
    a + b + 5
};
println(a); -# 0
println(b); -# 10
println(c) -# 15
```

## 6. Lists ({})
Lists in Zox are declared using curly braces {}. They can be indexed starting from 0, and you can modify elements directly.
```
let listA = {1, 2, 3, 4, 5};
let listB = {6, 7, 8, 9, 10};
let listC = {11, 12, 13, 14, 15};

let listD = listA + listB + listC;
println(listD); -# {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}

let c = len(listD);

#(c > 0) {
    c = c - 1;
    listD[c] = listD[c] * listC[c % len(listC)]
}
println(listD) -# {11, 24, 39, 56, 75, 66, 84, 104, 126, 150, 121, 144, 169, 196, 225}
```

Zox supports list slicing using the `[start:end]` syntax. The start index is inclusive, while the end index is exclusive.

```
let listA = {1, 2, 3, 4, 5};
println(listA[2:4]); -# {3, 4}
println(listA[:2]); -# {1, 2}
println(listA[2:]); -# {3, 4, 5}
println(listA[2]); -# 3
println(listA[:]); -# {1, 2, 3, 4, 5}
```

List operations:
- `list + list`: Concatenation - Combines two lists into one.
- `list * n`: Repetition - Repeats the list n times.
- `list * list`: Cartesian Product - Returns the Cartesian product of two lists.
- `list - list`: Difference - Removes elements of the second list from the first.
- `list & list`: Intersection - Returns the elements common to both lists.
- `list | list`: Union - Returns a list of elements from both lists without duplicates.
- `list ^ list`: Symmetric Difference - Returns the elements that are in either of the lists, but not in both.
- `list &+ n`: Element-wise Addition - Adds a given scalar to each element in the list.
- `list &- n`: Element-wise Subtraction - Subtracts a given scalar from each element in the list.
- `list &* n`: Element-wise Multiplication - Multiplies each element in the list by a given scalar.
- `list &/ n`: Element-wise Division - Divides each element in the list by a given scalar.
- `list &% n`: Element-wise Modulus - Computes the modulus of each element in the list by a given scalar.
- `list &** n`: Element-wise Power - Raises each element in the list to the power of a given scalar.
- `list &<< n`: Left Shift - Shifts each element in the list to the left by a given scalar.
- `list &>> n`: Right Shift - Shifts each element in the list to the right by a given scalar.
- `list &e n`: Element-wise AND - Performs a bitwise AND operation on each element in the list with a given scalar.
- `list &| n`: Element-wise OR - Performs a bitwise OR operation on each element in the list with a given scalar.
- `list &~`: Element-wise NOT - Performs a bitwise NOT operation on each element in the list.

## 7. Dictionaries ([])
Dictionaries in Zox use square brackets [] with the structure [key -> value].
For access, use curly brackets {}.
```
let person = [
    "name" -> "John";
    "age" -> 30;
    "city" -> "New York"
];
println(person{"name"}); -# John
person{"age"} = 31;
println(person{"age"}); -# 31

let keys = keys(person);
println(keys); -# {"name", "age", "city"}

let values = values(person);
println(values); -# {"John", 31, "New York"}
```

## 8. Functions ($)
Functions in Zox are declared using the `$` symbol followed by the function name. They can accept parameters and return the last expression evaluated.
```
$ add(a, b) {
    a + b
}
println(add(5, 10)) -# 15
```
Recursive functions can also be written:
```
$ fib(n) {
    ? (n <= 1) {
        n
    } : {
        fib(n - 1) + fib(n - 2)
    }
}
println(fib(10)) -# 55
```
You can implement iterative solutions as well, such as for the Fibonacci sequence:
```
$ fibIter(n) {
    let a = 0;
    let b = 1;
    let c = 0;
    @(let i = 0; i < n; i = i + 1) {
        c = a + b;
        a = b;
        b = c
    }
    a
}
println(fibIter(10)) -# 55
```

## 9. Comments

Zox supports single-line comments using the `-#` syntax. Anything after `-#` on a line is treated as a comment and ignored by the interpreter.

```
let x = 5; -# This is a comment
-# This entire line is a comment
let y = 10; -# Comments can be at the end of a line with code

-# Comments are useful for explaining code
-# or temporarily disabling code:
-# let z = 15;

let result = x + y; -# This adds x and y
```

Comments are completely ignored during the parsing and execution of the code. They are solely for the benefit of human readers to understand and document the code.

Key points about comments in Zox:
- They start with `-#` and continue to the end of the line.
- They can be on their own line or at the end of a line with code.
- They cannot be multi-line; each line of a multi-line comment needs its own `-#`.
- They are removed during the lexical analysis phase and do not appear in the AST.


## 10. Import
In Zox, the `~>` symbol is used to import modules, offering flexibility and support for aliasing, submodules, and selective imports.

```
~> module.math {abs, sqrt as squareRoot}; -# example of import submodules

println(abs(-16)); -# 16
println(squareRoot(16)); -# 4
```

## 11. Built-in Functions
Zox provides several built-in functions that are always available without the need for importing:
- `keys(dict)`: Returns a list of all keys in the given dictionary.
- `len(collection)`: Returns the number of elements in a list, characters in a string, or key-value pairs in a dictionary.
- `print(value)`: Outputs the given value to the console without adding a newline at the end.
- `println(value)`: Outputs the given value to the console and adds a newline at the end.
- `random()`: Generates a random floating-point number between 0 (inclusive) and 1 (exclusive).
- `randomInt(min, max)`: Generates a random integer within the specified range [min, max] (both inclusive).
- `values(dict)`: Returns a list of all values in the given dictionary.
- `sum(list)`: Calculates the sum of all numeric elements in the given list.
- `find(target, value)`: Returns the index of `value` in `target` (string/list), or checks if `value` exists as a key (dict). Returns -1 if not found. For dicts, a non-negative return only indicates presence.


These functions provide essential functionality for working with basic data structures and performing common operations in Zox programs.

## 12. Module Math
Zox has a built-in module called `math` that contains the following functions:
- `abs(x)`: Returns the absolute value of the number `x`.
- `sqrt(x)`: Returns the square root of the non-negative number `x`.
- `sin(x)`: Returns the sine of `x` (x in radians).
- `cos(x)`: Returns the cosine of `x` (x in radians).
- `tan(x)`: Returns the tangent of `x` (x in radians).
- `log(x)`: Returns the natural logarithm of the positive number `x`.
- `pow(x, y)`: Returns `x` raised to the power of `y`.
- `floor(x)`: Returns the largest integer less than or equal to `x`.
- `ceil(x)`: Returns the smallest integer greater than or equal to `x`.
- `round(x)`: Returns the nearest integer to `x`.
- `min(x, y)`: Returns the smaller of the two numbers `x` and `y`.
- `max(x, y)`: Returns the larger of the two numbers `x` and `y`.
- `lmin(list)`: Returns the smaller of the list.
- `lmax(list)`: Returns the larger of the list.
- `sum(list)`: Returns the sum of all numbers in the `list`.
- `average(list)`: Returns the arithmetic mean of the numbers in the `list`.
- `median(list)`: Returns the median value of the numbers in the `list`.

```
~> math {abs, sqrt as squareRoot};

println(abs(-16)); -# 16
println(squareRoot(16)); -# 4
```

## 13. File Module

Zox includes a built-in file module that provides basic file I/O operations. This module allows you to read from and write to files, making it possible to work with external data in your Zox programs.

The file module includes the following functions:
- `open(path, mode)`: Opens a file and returns a file handle. The path argument is a string specifying the file path, and mode is a string indicating the file access mode ("r" for read, "w" for write, "a" for append, etc.).
- `fRead(file)`: Reads the entire contents of the file and returns it as a string.
- `fReadLine(file)`: Reads a single line from the file. Returns -1 when it reaches the end of the file.
- `fWrite(file, content)`: Writes the given content to the file.
- `fSeek(file, position)`: Moves the file pointer to the specified position in the file.
- `fClose(file)`: Closes the file and releases the associated resources.
- `fExists(file)`: Returns true if the file exists otherwise returns false.
- `fDelete(file)`: Delete a file, returns true or false.
- `fCopy(file1, file2)`: Copy source file to destination file, returns true or false.
- `fMove(file1, file2)`: Move or rename a file, returns true or false.

```
~> file {open, fRead, fReadLine, fWrite, fSeek, fClose};

let f = open("teste.txt", "w");

fWrite(f, "Hello, world!\nThis is a new line.");

fClose(f);

f = open("teste.txt", "r");
let conteudo = fRead(f);
println(conteudo); -# Hello, world!\nThis is a new line.

fSeek(f, 0);

let linha = fReadLine(f);
#(linha != -1) {
    println(linha); -# Hello, world!; This is a new line.
    linha = fReadLine(f)
}

fClose(f)
```

## 14. Strings

In Zox, strings are immutable sequences of characters. This means that once a string is created, it cannot be modified. However, operations can be performed on strings to create new strings. Zox provides a rich set of features for working with strings, including indexing, slicing, concatenation, repetition, and more.

### String Indexing and Slicing
You can access individual characters in a string by their index, and you can slice strings to extract substrings. String indices start at 0, and slicing works similarly to lists.

```
let str = "Hello, world!";
println(str[0]);      -# H
println(str[-1]);     -# !
println(str[7:12]);   -# world
```

### String Concatenation
You can concatenate two or more strings using the + operator to form a new string.

```
let str1 = "Hello, ";
let str2 = "world!";
let result = str1 + str2;
println(result); -# Hello, world!
```
### String Repetition
The `*` operator allows you to repeat a string a specified number of times, creating a new string.

```
let str = "Repeat ";
println(str * 3); -# Repeat Repeat Repeat
```

### Finding Substrings
To find the position of a substring within a string, use the `find` function. This function returns the index of the first occurrence of the substring or -1 if the substring is not found.
```
let str = "Hello, world!";
let index = find(str, "world");
println(index); -# 7
index = find(str, "abacate");
println(index); -# -1
```

### Getting the Length of a String
You can determine the length of a string (i.e., the number of characters it contains) using the `len` function.
```
let str = "Hello, world!";
let length = len(str);
println(length); -# 13
```

### Removing Substrings
To remove all occurrences of a substring from a string, you can use the - operator. This creates a new string with the specified substring removed.
```
let str = "Hello, world!";
let newStr = str - "world";
println(newStr); -# Hello, !
```

## Implementation Details

### 1. Lexical Analysis
The lexer (lexer.c) breaks down the source code into tokens. It demonstrates:
- How to identify and categorize different elements of the language
- Handling of whitespace and comments
- Error reporting for invalid characters

### 2. Parsing

The parser (parser.c) constructs an Abstract Syntax Tree (AST) from the token stream. It showcases:
- Recursive descent parsing techniques
- Handling of operator precedence
- Construction of different AST node types

### 3. Abstract Syntax Tree (AST)
The AST structure (ast.h and ast.c) represents the hierarchical structure of the program. It illustrates:
- How different language constructs are represented in memory
- The visitor pattern for traversing and operating on the AST

### 4. Symbol Table and Environment
The environment implementation (env.c) demonstrates:
- How variables are stored and looked up
- Scope management (local vs. global variables)
- Implementation of a simple hash table for efficient variable lookup

### 5. Interpreter
The interpreter (eval.c) shows how to:
- Traverse the AST and execute the program
- Implement different operations and language constructs
- Handle runtime errors

### 6. Memory Management
Throughout the implementation, you can observe:
- Allocation and deallocation of AST nodes, environments, and runtime values
- Strategies for avoiding memory leaks in an interpreter

## Educational Value
By studying Zox's implementation, learners can:
- Understand the pipeline from source code to execution.
- Learn about different data structures used in language implementation.
- Explore techniques for efficient symbol table management.
- Gain insights into how to implement common programming language features.
- Practice C programming in a complex, real-world project.
