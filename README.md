# lowC

A small C-like systems language and compiler.

## Overview

lowC is a compact compiled language with a clean, C-inspired syntax. It provides explicit control over data representation and memory while keeping the language and compiler straightforward.

lowC supports functions, structs, pointers, arrays, explicit-width integers, and direct control flow. The compiler produces explicit machine code and does not use an intermediate representation (IR).

The generated assembly prioritizes clarity and explicitness, with detailed commentary rather than aggressive optimization.

## Build and Run

Build the size-optimized release compiler with `make` or `make release`:

```sh
make
```

This builds `build/lowc` with size optimization, link-time optimization, dead
section removal, and stripped symbols. Use `make debug` to build an
unoptimized debug executable separately at `build/debug/lowc`.
The release and portable builds omit compiler unwind tables to reduce their
size; this can limit stack unwinding in debuggers and crash backtraces.
`make install` always installs the release executable from `build/lowc`.
Use `make portable` to build a statically linked Linux executable at
`build/portable/lowc`; it remains specific to the current CPU architecture.
On the x86-64 Alpine GCC 15.2/musl toolchain, these builds are about 53.7 KiB
and 118.1 KiB respectively; exact sizes vary by toolchain and target.

New to lowC? Run `make quick_start` for a hand-holding introduction to the language, followed by compiling and running a small example. The guide is also available in [`QUICK_START.md`](QUICK_START.md).

Run `make example` to build and play through a small multi-file trail challenge.
It exercises imported modules, structs, arrays, pointers and pointer arithmetic,
globals, functions, loops, and conditionals:

```console
$ make example
Trail challenge complete! Score: 42 (expected 42)
```

The lowC source is split across `example/trail/point.lowc`,
`example/trail/route.lowc`, and `example/trail/main.lowc`. A small C harness
prints and checks the score.

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

Executable output is assembled and linked with the system C compiler driver
(`cc` by default; set `CC` to select another driver). Executable programs must
define exactly one `void entrypoint(void)`. The compiler supplies the platform
`main` wrapper.

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

Functions that take no parameters must declare `void` in the parameter list.
Writing an empty parameter list in a function definition is a parser error;
calls to such functions still use empty parentheses.

```c
void foo(void) {
    return;
}
```

Functions may call other functions.

**Function overloading is not supported.**

### Entrypoint

Every executable program must define exactly one `entrypoint` function.

`entrypoint` always returns `void` and takes no parameters.

```c
void entrypoint(void) {
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

void entrypoint(void) {
    s64 result = add(10, 20);
}
```

Imports are resolved relative to the importing file and can be used for:

* Functions
* Structs
* Variables

Only the names listed by an import are visible to that source file. Declarations
in a module remain available within that module, but imports are not re-exported;
import a dependency directly to use its names from another file.

## Standard library

`make install` also installs public standard-library modules beneath
`$(PREFIX)/share/lowc/public` (by default,
`/usr/local/share/lowc/public`). Its `print.lowc` module provides string
output on Linux x86-64:

```c
#include "/usr/local/share/lowc/public/print.lowc" : "print";

void entrypoint(void) {
    print("Hello, World\n");
}
```

String literals evaluate to `u8 *` and are stored as read-only byte data. The
public `print` function calculates their length with the public `strlen`
helper, so output stops at the first NUL byte. `print` writes to standard
output via the Linux x86-64 `write` syscall; it has no error return in this
initial API.

`print_u1`, `print_u2`, `print_u4`, `print_u8`, `print_u16`, `print_u32`, and
`print_u64` print unsigned integers in decimal. `print_s1`, `print_s2`,
`print_s4`, `print_s8`, `print_s16`, `print_s32`, and `print_s64` do the same
for signed integers. Numeric print functions do not add a newline:

```c
#include "/usr/local/share/lowc/public/print.lowc" :
    "print_u64", "print_s64", "print";

void entrypoint(void) {
    print_u64(42);
    print_s64(-7);
    print("\n");
}
```

Use `strlen` directly by importing `string.lowc`:

```c
#include "/usr/local/share/lowc/public/string.lowc" :
    "strlen", "parse_u64", "strcmp", "strcpy", "strcat", "strdup";
#include "/usr/local/share/lowc/public/memory.lowc" : "dealloc";

u64 string_example(void) {
    u8 *copy = strdup("Hello");
    u64 length = strlen(copy);
    dealloc(copy);
    return length;
}
```

`parse_u64(text, &value)` returns true for a non-empty sequence of decimal
digits that fits in `u64`, and writes the parsed value only on success. It
accepts leading zeroes but rejects signs, whitespace, other characters, and
overflow.

`strcmp` returns a negative value, zero, or a positive value for less-than,
equal, or greater-than strings. `strcpy` and `strcat` return the destination;
the caller must provide enough space. `strdup` returns a heap-allocated copy
that should be released with `dealloc`.

The `memory.lowc` module provides Linux x86-64 `mmap`-backed allocations:

```c
#include "/usr/local/share/lowc/public/memory.lowc" : "alloc", "dealloc";

u1 allocation_example(void) {
    u8 *buffer = alloc(128);
    buffer[0] = 42;
    return dealloc(buffer);
}
```

Allocation failure terminates the process with status 1. `dealloc` returns
whether the underlying unmap succeeded; only pass it pointers returned by
`alloc` or `strdup`, and release each allocation once.

The `hashmap.lowc` module provides an open-addressed `u64`-to-`u64` map with
linear probing and automatic growth. Import `HashMap`, `hashmap_init`,
`hashmap_set`, `hashmap_get`, `hashmap_remove`, and `hashmap_deinit`. Initialize
the struct before use; `hashmap_get` writes into its output pointer and returns
whether the key existed:

```c
#include "/usr/local/share/lowc/public/hashmap.lowc" :
    "HashMap", "hashmap_init", "hashmap_set",
    "hashmap_get", "hashmap_remove", "hashmap_deinit";

u1 hashmap_example(void) {
    struct HashMap map;
    u64 value;

    hashmap_init(&map, 8);
    hashmap_set(&map, 42, 100);
    if (!hashmap_get(&map, 42, &value) || value != 100) {
        hashmap_deinit(&map);
        return 0;
    }
    return hashmap_deinit(&map);
}
```

The initial capacity is raised to at least eight and the map grows before its
load reaches 75%. Keys and values are unsigned 64-bit integers; `hashmap_remove`
marks removed slots for reuse, and `hashmap_deinit` releases the map's backing
allocations. Do not use the map after deinitializing it.

Use your configured `PREFIX` in standard-library import paths when it differs
from `/usr/local`.

## Formatting

`lowc --format file.lowc` overwrites the input file with formatted source. It
requires `clang-format` in `PATH` and always uses this fixed style; it does not
read `.clang-format` or accept style overrides:

```yaml
BasedOnStyle: Google
IndentWidth: 8
ColumnLimit: 80
SpaceBeforeParens: ControlStatements
SpacesInParentheses: false
AlignConsecutiveAssignments: true
SortIncludes: true
SpacesBeforeTrailingComments: 0
AlwaysBreakTemplateDeclarations: false
SpaceAfterCStyleCast: false
DerivePointerAlignment: false
PointerAlignment: Left
SeparateDefinitionBlocks: Always
UseTab: Never
```

Formatting cannot be combined with `--tokens`, `--ast`, `--asm`, `-O1`, or
`-o`.

## Inline assembly

Inline assembly is a statement and currently supports the x86-64 backend's
Intel-syntax assembly. Outputs precede inputs; each operand names a fixed
register, and `%0`, `%1`, and subsequent placeholders refer to outputs first,
then inputs:

```c
u64 result;
asm("mov %0, %1" : "=rax"(result) : "rdi"(42) : "rcx");
```

Output constraints use `"=register"(lvalue)` and input constraints use
`"register"(expression)`. The supported 64-bit general-purpose registers are
`rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, and `r8` through `r15`. The clobber
list names registers the template modifies; `"memory"` declares memory
effects and `"cc"` declares condition-code effects. Output registers are
already treated as modified. Use `%%` for a
literal percent sign, and write empty sections explicitly (for example,
`asm("nop" : : :);`). The compiler preserves
callee-saved registers named as operands or clobbers. `rsp` and `rbp` are
reserved and cannot be operands or clobbers; templates must not modify them.
Other code-generation targets reject inline assembly.
