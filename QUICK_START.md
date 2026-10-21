# lowC quick start

This guide introduces lowC by building a small program step by step. It assumes you are in the repository root and have `make` plus a C compiler driver such as `cc` installed.

You can have the repository build and run the included sample after printing this guide:

```sh
make quick_start
```

## 1. Build the compiler

```sh
make
```

This creates `build/lowc`. Ask the compiler to list its command-line options with:

```sh
build/lowc --help
```

## 2. Write a first program

Create a file called `hello.lowc`:

```c
foreign c s4 printf(u1 *format, u1 *text);

void entrypoint(void) {
    printf("Hello, %s\n", "World");
    return;
}
```

`entrypoint` is where an executable begins; it must take no parameters and
return `void`. The `foreign c` declaration describes the C function signature;
the default C compiler driver links `printf` from the standard library.

## 3. Compile and run it

```sh
build/lowc -O1 -o hello hello.lowc
./hello
```

`-o hello` chooses the executable's name. `-O1` enables lightweight integer constant folding; omit it to use the straightforward unoptimized path. If you leave off `-o`, the executable is named `a.out`.

The compiler asks the system C compiler driver to assemble and link the generated x86-64 code. By default it uses `cc`; set `CC` to use another compatible driver:

```sh
CC=clang build/lowc -o hello hello.lowc
```

## 4. Learn the basic types and variables

lowC has signed and unsigned integers whose type suffix gives the size in
bytes. Supported sizes are 1, 2, 4, and 8 bytes, with 8 bytes as the maximum:

```c
s1 small_signed;
u4 count = 3;
s8 total = 40;
```

The byte width is part of the type name. Assignments use `=`:

```c
total = total + 2;
```

Integer arithmetic includes `+`, `-`, `*`, `/`, and `%`. Comparisons such as
`==`, `<`, and `>=` produce a one-byte `u1` result.

## 5. Add decisions and loops

`if` and `else` choose between blocks:

```c
if (total == 42) {
    total = total + 1;
} else {
    total = 0;
}
```

A `for` loop has initialization, condition, and increment sections:

```c
for (s8 i = 0; i < 3; i = i + 1) {
    total = total + i;
}
```

## 6. Use arrays and pointers

Array indexing works directly:

```c
s8 values[3];
values[0] = 10;
values[1] = 20;
```

Arrays do not implicitly convert to pointers. Take an element's address explicitly:

```c
s8 *pointer;
pointer = &values[0];
*pointer = *pointer + 1;
```

Pointer arithmetic scales by the size of the pointed-to type. For example, `pointer + 1` addresses the next `s8`, not the next byte.

## 7. Define a struct

```c
struct Point {
    s8 x;
    s8 y;
};

struct Point point;
point.x = 10;
point.y = 20;

struct Point *point_pointer;
point_pointer = &point;
point_pointer->x = 30;
```

Use `.` with a struct value and `->` with a pointer to a struct.

## 8. Split code into modules

Every `.lowc` file can be imported. For example, if `math.lowc` defines `add`, another file can import and call it:

```c
#include "math.lowc" : "add";

void entrypoint(void) {
    s8 result = add(20, 22);
    return;
}
```

The compiler resolves the imported path relative to the file containing the import.

## 9. Inspect assembly and run tests

Print the generated Intel-syntax assembly instead of building an executable:

```sh
build/lowc --asm hello.lowc
```

Run the repository's lexer, parser, assembler, and executable checks with:

```sh
./example/tests/run_tests.sh
```

For a fuller collection of examples, browse `example/tests/`.
