# lowC

A small C-like systems language and compiler.

## Overview

lowC is a compact compiled language with a clean, C-inspired syntax. It provides explicit control over data representation and memory while keeping the language and compiler straightforward.

lowC supports functions, structs, pointers, arrays, explicit-width integers, and direct control flow. The compiler produces explicit machine code and does not use an intermediate representation (IR).

The generated assembly prioritizes clarity and explicitness, with detailed commentary rather than aggressive optimization.

## Build and Run

Build the compiler with `make`:

```sh
make
```

Install the compiler as `/usr/local/bin/lowc` (use `sudo` if needed), or choose a different prefix:

```sh
sudo make install
make install PREFIX="$HOME/.local"
```

`DESTDIR` can stage an install without changing the configured prefix:

```sh
make install DESTDIR=/tmp/lowc-stage PREFIX=/usr
```

By default, lowC compiles a source file to an executable named `a.out`:

```sh
build/lowc program.lowc
./a.out
```

Use `-o` to choose the executable path, `-O1` to enable lightweight integer constant folding, or `--asm` to print assembly instead of building an executable:

```sh
build/lowc -O1 -o program program.lowc
build/lowc --asm program.lowc
```

Executable output is assembled and linked with the system C compiler driver (`cc` by default; set `CC` to select another driver). Executable programs must define exactly one `void entrypoint()` with no parameters. The compiler supplies the platform `main` wrapper.

Run the compiler and generated-code checks with:

```sh
./example/tests/run_tests.sh
```

The suite parses every example, assembles generated output, and checks executable behavior with both optimized and unoptimized assembly.

## Motivation

lowC originally began as a subset of C, but has evolved into its own C-like language.

The goal is not to reproduce C or preserve its historical design decisions. lowC keeps the parts of C that provide a useful low-level programming model while defining its own simpler and more consistent language rules.

The language is intentionally small, but small does not mean it is toy-oriented. Its design is focused on providing a practical, explicit systems programming model with a straightforward path from source code to machine code.

---

## Type System

### Unsigned Integers

**1, 2, 4, 8, 16, 32, and 64-bit:**

```c
u1  u2  u4  u8  u16  u32  u64
```

### Signed Integers

**1, 2, 4, 8, 16, 32, and 64-bit:**

```c
s1  s2  s4  s8  s16  s32  s64
```

### Other Types

```c
void
```

---

## Variables

### Declaration

```c
u64 x;
u64 x = 42;
```

### Assignment

```c
x = 10;
```

---

## Operators

### Arithmetic

```c
+  -  *  /  %
```

**Unary:**

```c
+x
-x
```

### Comparisons

```c
==  !=  <  <=  >  >=
```

Comparisons return `u1`.

### Logic

```c
!       // NOT
&&      // AND 
||      // OR 
```

---

## Pointers

### Declaration and Usage

```c
u64 *p;

p = &x;

*p;
*p = 42;
```

Pointer arithmetic is automatically scaled by the size of the pointed-to type.

---

## Arrays

### Declaration and Access

```c
u64 values[10];

values[0] = 42;
```

### Array-to-Pointer Conversion

lowC does **not** perform implicit array-to-pointer decay.

An array expression retains its array type and cannot be implicitly assigned to a pointer:

```c
s64 values[5];
s64 *p;

p = values;        // invalid
```

To obtain a pointer to an array element, use the address-of operator explicitly:

```c
p = &values[0];    // valid
```

Array indexing is equivalent to pointer arithmetic followed by dereferencing:

```c
values[1];
```

is equivalent to:

```c
*(values + 1);
```

Pointer arithmetic is scaled by the size of the pointed-to type.

---

## Structs

### Definition and Usage

```c
struct Point {
    s64 x;
    s64 y;
};

struct Point p;

p.x = 10;
p.y = 20;
```

### Pointer Access

```c
struct Point *p;

p->x = 10;
```

### Arrays of Structs

Struct types can be used as array element types:

```c
struct Point points[10];

points[0].x = 10;
points[0].y = 20;

points[1].x = 30;
points[1].y = 40;
```

Array indexing is performed before member access, so points[1].x accesses the x member of the second Point in the array.

---

## Functions

### Basic Functions

```c
s64 add(s64 a, s64 b) {
    return a + b;
}
```

### Void Functions

```c
void foo() {
    return;
}
```

Functions may call other functions.

**Function overloading is not supported.**

### Entrypoint

Every executable program must define exactly one `entrypoint` function.

`entrypoint` always returns `void` and takes no parameters.

```c
void entrypoint() {
    // Program execution begins here.
}
```

Unlike C, lowC does not use `main()` as the program entrypoint.

---

## Control Flow

### Conditionals

```c
if (condition) {
    ...
}
```

```c
if (condition) {
    ...
} else {
    ...
}
```

### Loops

```c
for (init; condition; increment) {
    ...
}
```

### Return Statements

Functions returning a value use:

```c
return expression;
```

Void functions use:

```c
return;
```

---

## Modules

Every `.lowc` file can be used as a module. lowC does not use separate header files.

Modules can explicitly import functions, structs, and variables:

```c
#include "math.lowc" : "add", "sqrt"
```

For example:

**math.lowc**

```c
s64 add(s64 a, s64 b) {
    return a + b;
}

s64 sqrt(s64 x) {
    ...
}
```

**main.lowc**

```c
#include "math.lowc" : "add";

void entrypoint() {
    s64 result = add(10, 20);
}
```

Imports are resolved relative to the importing file and can be used for:

* Functions
* Structs
* Variables
