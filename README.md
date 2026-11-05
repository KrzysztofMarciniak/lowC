# lowC

**Version 3.1**

A small C-like systems language and compiler.

![Logo](Logo.png)

## Table of Contents

- [Overview](#overview)
- [Build and Run](#build-and-run)
  - [A complete lowC program](#a-complete-lowc-program)
  - [Call a function from your own C file](#call-a-function-from-your-own-c-file)
  - [How lowC differs from C](#how-lowc-differs-from-c)
- [Practical examples](#practical-examples)
  - [Sum an array](#sum-an-array)
  - [List command-line arguments](#list-command-line-arguments)
  - [Count bytes in a file](#count-bytes-in-a-file)
  - [Store values in a hash map](#store-values-in-a-hash-map)
  - [Fetch a local HTTP page](#fetch-a-local-http-page)
- [Motivation](#motivation)
- [Type System](#type-system)
  - [Unsigned Integers](#unsigned-integers)
  - [Signed Integers](#signed-integers)
  - [Other Types](#other-types)
- [Variables](#variables)
  - [Declaration](#declaration)
  - [Assignment](#assignment)
- [Operators](#operators)
  - [Arithmetic](#arithmetic)
  - [Comparisons](#comparisons)
  - [Logic](#logic)
- [Pointers](#pointers)
  - [Declaration and Usage](#declaration-and-usage)
- [Arrays](#arrays)
  - [Declaration and Access](#declaration-and-access)
  - [Array-to-Pointer Conversion](#array-to-pointer-conversion)
- [Structs](#structs)
  - [Definition and Usage](#definition-and-usage)
  - [Pointer Access](#pointer-access)
  - [Arrays of Structs](#arrays-of-structs)
- [Functions](#functions)
  - [Basic Functions](#basic-functions)
  - [Multiple Return Values](#multiple-return-values)
  - [Void Functions](#void-functions)
  - [Entrypoint](#entrypoint)
  - [Calling C functions](#calling-c-functions)
- [Control Flow](#control-flow)
  - [Conditionals](#conditionals)
  - [Loops](#loops)
  - [Return Statements](#return-statements)
- [Modules](#modules)
- [Standard library](#standard-library)
- [Formatting](#formatting)
- [Inline assembly](#inline-assembly)

## Overview

lowC is a compact compiled language with a clean, C-inspired syntax. It provides explicit control over data representation and memory while keeping the language and compiler straightforward.

lowC supports functions, structs, pointers, arrays, explicit byte-width integers, and direct control flow. Its IR tracks typed expression values and control-flow basic blocks, selects referenced imported declarations, and performs constant folding before x86-64 code generation.

The IR optimizer folds constant expressions. Release executables are linked
with `-O3`, LTO, dead-section removal, and stripped symbols; these driver
optimization flags also optimize any C sources compiled alongside lowC code.

## Build and Run

Build the optimized release compiler with `make` or `make release`:

```sh
make
```

This builds `build/lowc` with `-Os`, link-time optimization, dead section
removal, and stripped symbols. Use `make debug` to build an
unoptimized debug executable separately at `build/debug/lowc`.
The release and portable builds omit compiler unwind tables to reduce their
size; this can limit stack unwinding in debuggers and crash backtraces.
`make install` always installs the release executable from `build/lowc`.
Use `make portable` to build a statically linked Linux executable at
`build/portable/lowc`; it remains specific to the current CPU architecture.
Release builds favor a smaller executable and remain portable across x86-64
CPUs; no host-specific instruction set is enabled.

### Hello, World!

```c
import public/print.lowc : print;

void entrypoint(void) { 
	print("Hello, World\n");
	return;
}
```

Compile and run it:

```sh
build/lowc -o hello hello.lowc
./hello
```

### Call a function from your own C file

Keep the declaration and the C definition in agreement. `c-example.lowc`:

```c
foreign c u8 add_from_c(u8 left, u8 right); // Matches helper.c's signature.
foreign c s4 puts(u1 *text);

void entrypoint(void) {
    u8 answer = add_from_c(19, 23);
    if (answer == 42) {
        puts("The C function returned 42");
    }
    return;
}
```

`helper.c`:

```c
#include <stdint.h>

uint64_t add_from_c(uint64_t left, uint64_t right)
{
    return left + right;
}
```

Pass both source files to lowc; it compiles and links the C file with the
program:

```sh
build/lowc -o c-example c-example.lowc helper.c
./c-example
```

Expected output:

```text
The C function returned 42
```

For a prebuilt library, use `-L<path>` and `-l<name>`. The foreign interface
uses fixed argument types; general variadic calls such as arbitrary `printf`
calls are not supported.

### How lowC differs from C

| In C | In lowC |
| --- | --- |
| `int main(void)` is the usual program entrypoint. | Define exactly one `void entrypoint(void)`; the compiler supplies the platform `main` wrapper. |
| `long count;` may be left uninitialized. | Every variable needs an initializer, for example `s8 count = 0;`. |
| `int64_t` and `uint32_t` describe bit widths. | `s8` and `u4` name signed/unsigned integers by **byte width**. |
| Arrays commonly decay to pointers in expressions. | Arrays keep their array type; write `&values[0]` to get an element pointer. |
| C headers declare APIs through `#include`. | Declare C calls with `foreign c`; lowC does not parse C headers. |
| C functions return one value. | LowC functions may return several values using tuple assignment (the C ABI does not support this lowC feature). |
| A `void` function can reach its closing brace. | Every lowC function needs an explicit `return` on every normal path. |
| C leaves argument evaluation order unspecified for many operators. | lowC evaluates function arguments from right to left. |

In C, array-to-pointer decay lets a pointer take the array's first-element
address implicitly:

```c
int64_t values[3] = {0};
int64_t *pointer = values; // C implicitly uses &values[0].
```

In lowC, write the address explicitly:

```c
s8 values[3] = 0; // Arrays and structs can be zero-initialized this way.
s8 *pointer = 0;
pointer = &values[0]; // Required; `pointer = values` is invalid in lowC.
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

To install lowC and its standard library under your home directory, use a
user-owned prefix:

```sh
make install PREFIX="$HOME/.lowc" LOWC_STDLIB_DIR="$HOME/.lowc/share/lowc"
```

Add the compiler to your shell's `PATH`, for example in `~/.profile`:

```sh
export PATH="$HOME/.lowc/bin:$PATH"
```

You can add your own modules under `~/.lowc/share/lowc/public` and import
them with the `public/` prefix. To use a different library tree, set
`LOWC_STDLIB_DIR` to its root directory, the directory containing `public`:

```sh
export LOWC_STDLIB_DIR="$HOME/.lowc/share/lowc"
```

By default, lowC compiles a source file to an executable named `a.out`:

```sh
build/lowc program.lowc
./a.out
```

Normal executable builds optimize the compiler's IR by default. Use
`-optimize` or its alias `-release` to explicitly request release linking:
`-Os`, LTO, dead-section removal, and stripped symbols. The IR optimizer folds
constant expressions, simplifies constant branches, propagates scalar constants
through straight-line code, and precomputes simple pure multi-return calls
with constant arguments. It also removes blocks made unreachable by branch
simplification and unused, proven-pure expression statements; calls, stores,
inline assembly, and potentially trapping or indirect memory operations are
preserved. Each function gets its own linker section so unused code can be
discarded. The system compiler's optimization flags also apply to C sources
compiled alongside lowC.
`-debug` emits source-mapped debug information, retains symbols, and disables
optimization unless combined with `-optimize` or `-release`. There are no
`-O0`/`-O1` levels.
Use `-o` to choose the executable path, or `--asm` to print assembly:

```sh
build/lowc -optimize -o program program.lowc
build/lowc -debug -o program program.lowc
build/lowc --asm program.lowc
```

Executable output is assembled and linked with the system C compiler driver
(`cc` by default; set `CC` to select another driver). Executable programs must
define exactly one `void entrypoint(void)`. The compiler supplies the platform
`main` wrapper.

Run the compiler and generated-code checks with:

```sh
make test
make test TEST=frontend
```

The suite parses and assembles every smoke example, then runs focused test
areas. Set `TEST` to `all`, `smoke`, `async`, `frontend`, `modules`, `optimization`,
`semantic`, `standard-library`, or `inline-asm` to select an area.

Smoke examples are in `tests/smoke/examples`; per-suite runners and fixtures
are in `tests/suites/<suite>/`, with shared support in `tests/support/`.

## Practical examples

These complete programs can be saved as the named `.lowc` files and compiled
with `build/lowc`. They use the public standard library installed by
`make install`; set `LOWC_STDLIB_DIR` if it is in a custom location.

### Sum an array

This small program initializes an array, loops over it, and prints the total.
Save it as `sum.lowc`:

```c
import public/print.lowc : print, print_s8;

void entrypoint(void) {
    s8 values[5] = 0;
    s8 total = 0;
    s8 index = 0;

    values[0] = 1;
    values[1] = 2;
    values[2] = 3;
    values[3] = 4;
    values[4] = 5;

    for (index = 0; index < 5; index = index + 1) {
        total = total + values[index];
    }

    print("sum = ");
    print_s8(total);
    print("\n");
    return;
}
```

```sh
build/lowc -o sum sum.lowc
./sum
```

Output:

```text
sum = 15
```

### List command-line arguments

`args_count()` includes the executable at index zero. This program starts at
index one to print only the arguments supplied by the user. Save it as
`list-args.lowc`:

```c
import public/args.lowc : args_count, args_get;
import public/print.lowc : print;

void entrypoint(void) {
    u8 count = 0;
    u8 index = 0;
    u1 found = 0;
    u1 *argument = 0;

    count = args_count();
    for (index = 1; index < count; index = index + 1) {
        (found, argument) = args_get(index);
        if (found) {
            print(argument);
            print("\n");
        }
    }
    return;
}
```

```sh
build/lowc -o list-args list-args.lowc
./list-args alpha "two words"
```

Output:

```text
alpha
two words
```

### Count bytes in a file

This program reads a file in chunks and continues until `file_read` reports
end-of-file. Save it as `count-bytes.lowc`:

```c
import public/args.lowc : args_count, args_get;
import public/file.lowc : File, file_open_read, file_read, file_close;
import public/print.lowc : print, print_s8;

void entrypoint(void) {
    struct File file = 0;
    u8 count = 0;
    u1 found = 0;
    u1 *path = 0;
    u1 buffer[256] = 0;
    s8 result = 1;
    s8 total = 0;

    count = args_count();
    if (count != 2) {
        print("usage: count-bytes FILE\n");
        return;
    }

    (found, path) = args_get(1);
    if (found == 0 || file_open_read(path, &file) != 0) {
        print("could not open file\n");
        return;
    }

    for (; result > 0;) {
        result = file_read(&file, &buffer[0], 256);
        if (result < 0) {
            file_close(&file);
            print("file read failed\n");
            return;
        }
        total = total + result;
    }

    if (file_close(&file) != 0) {
        print("file close failed\n");
        return;
    }

    print("bytes: ");
    print_s8(total);
    print("\n");
    return;
}
```

```sh
build/lowc -o count-bytes count-bytes.lowc
printf 'lowC\n' > sample.txt
./count-bytes sample.txt
```

Output:

```text
bytes: 5
```

### Store values in a hash map

The standard-library map stores `u8` keys and values. This example records
inventory quantities and looks up an item by its numeric ID. Save it as
`inventory.lowc`:

```c
import public/hashmap.lowc : HashMap, hashmap_init, hashmap_set, hashmap_get,
    hashmap_deinit;
import public/print.lowc : print, print_u8;

void entrypoint(void) {
    struct HashMap inventory = 0;
    u1 found = 0;
    u8 quantity = 0;

    hashmap_init(&inventory, 8);
    hashmap_set(&inventory, 1001, 4);
    hashmap_set(&inventory, 1002, 9);

    (found, quantity) = hashmap_get(&inventory, 1001);
    if (found) {
        print("item 1001 quantity: ");
        print_u8(quantity);
        print("\n");
    }

    hashmap_deinit(&inventory);
    return;
}
```

```sh
build/lowc -o inventory inventory.lowc
./inventory
```

Output:

```text
item 1001 quantity: 4
```

Call `hashmap_deinit` when finished with the map so its backing allocations
are released.

### Fetch a local HTTP page

This example fetches a page over plain HTTP and prints its status and body.
It uses a local server so it does not depend on an external host. Save it as
`fetch-page.lowc`:

```c
import public/http.lowc : http_get;
import public/print.lowc : print, print_s4, print_s8;

void entrypoint(void) {
    u1 body[4096] = 0;
    s8 body_length = 0;
    s4 status_code = 0;

    (body_length, status_code) = http_get(
        "127.0.0.1", 8000, "/", &body[0], 4095
    );
    if (body_length < 0) {
        print("request failed: ");
        print_s8(body_length);
        print("\n");
        return;
    }

    body[body_length] = 0;
    print("HTTP status: ");
    print_s4(status_code);
    print("\n");
    print(&body[0]);
    print("\n");
    return;
}
```

Start a local HTTP server in one terminal:

```sh
python3 -m http.server 8000 --bind 127.0.0.1
```

Then compile and run the client in another terminal:

```sh
build/lowc -o fetch-page fetch-page.lowc
./fetch-page
```

The server's HTML is printed after the `HTTP status: 200` line. The standard
library HTTP client supports plain HTTP and chunked response decoding, but not
TLS or redirects.

## Motivation

lowC originally began as a subset of C, but has evolved into its own C-like language.

The goal is not to reproduce C or preserve its historical design decisions. lowC keeps the parts of C that provide a useful low-level programming model while defining its own simpler and more consistent language rules.

The language is intentionally small, but small does not mean it is toy-oriented. Its design is focused on providing a practical, explicit systems programming model with a straightforward path from source code to machine code.

---

## Type System

### Unsigned Integers

Types are named by their size in bytes. Supported sizes are 1, 2, 4, and 8
bytes (up to 64 bits):

```c
u1  u2  u4  u8
```

### Signed Integers

The same byte sizes are supported for signed integers:

```c
s1  s2  s4  s8
```

### Other Types

```c
void
```

---

## Variables

### Declaration

Every variable must have an initializer. Use `= 0` for an explicit zero
initial value; arrays and struct values can also be zero-initialized this way.

```c
s8 x = 0; // `s8` is a signed 8-byte integer.
u4 y = 42; // `u4` is an unsigned 4-byte integer.
```

Unlike C, these declarations cannot omit an initializer; use `= 0` when the
initial value should be zero.

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

Comparisons return `u1`, an unsigned one-byte value.

### Logic

```c
!       // NOT
&&      // AND 
||      // OR 
```

Integer literals contain decimal digits and have type `s8`. Assignments allow
conversion between integer types. Integer arithmetic produces `s8`; comparisons
produce `u1`. The current x86-64 backend performs division, remainder, and
ordered comparisons as signed operations, including for `u` operands. Unsigned
division and ordering therefore do not yet have reliable unsigned semantics.
Literal range, overflow, and division-by-zero behavior are not fully specified.

`&&` and `||` short-circuit and require integer operands. `if` and `for`
conditions currently do not require an integer type, and generated code tests
the result against zero. Use integer conditions; behavior for non-integer
conditions is not defined.

---

## Pointers

### Declaration and Usage

```c
u8 value = 0;
u8 *p = 0; // Zero is accepted as a null pointer initializer.
p = &value; // Take the address explicitly.
*p = 42; // Dereference the pointer to update value.
```

Pointer arithmetic is automatically scaled by the size of the pointed-to type.
Subtracting pointers of the same type produces a signed element distance.
Pointer dereferences and array indexing have no runtime null or bounds checks;
invalid memory access is the program's responsibility.
The literal `0` is accepted when initializing a pointer; integer-to-pointer
assignment is otherwise not allowed.

---

## Arrays

### Declaration and Access

```c
u8 values[10] = 0;

values[0] = 42; // Set the first element.
```

Array lengths must be decimal integer literals; expressions and negative
lengths are rejected. Zero is accepted. Indexing does not check that an index
is within the array.

### Array-to-Pointer Conversion

lowC does **not** perform implicit array-to-pointer decay.

An array expression retains its array type and cannot be implicitly assigned to a pointer:

```c
s8 values[5] = 0;
s8 *p = 0;

p = values; // Invalid: lowC arrays do not decay to pointers.
```

To obtain a pointer to an array element, use the address-of operator explicitly:

```c
p = &values[0]; // Valid: take the first element's address.
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
    s8 x;
    s8 y;
};

struct Point p = 0; // Explicitly zero-initialize both fields.

p.x = 10;
p.y = 20;
```

On x86-64, fields are aligned to their type's size-based alignment (pointers
are 8-byte aligned), and the struct size is rounded up to its maximum field
alignment. This layout is target-specific and should not be assumed to match
every C compiler's ABI.

### Pointer Access

```c
struct Point point = 0;
struct Point *p = 0;

p = &point; // Initialize the pointer before dereferencing it.
p->x = 10; // `->` accesses a field through a pointer.
```

### Arrays of Structs

Struct types can be used as array element types:

```c
struct Point points[10] = 0;

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
s8 add(s8 a, s8 b) {
    return a + b; // Return a value explicitly.
}
```

### Multiple Return Values

Functions can declare multiple return types and return a matching list of
expressions. Call them with tuple destructuring assignment:

```c
s8, s8 divide(s8 numerator, s8 denominator) {
    return numerator / denominator, numerator % denominator;
}

void entrypoint(void) {
    s8 quotient = 0; // Initialize each assignment target.
    s8 remainder = 0;

    (quotient, remainder) = divide(37, 5); // C functions return one value.
    return;
}
```

Return values and assignment targets are matched by position, and their count
and types are checked. A multi-return call must be used directly in tuple
assignment; tuples are not standalone values. `void` can only be used as the
single return type. `foreign c` functions continue to have exactly one return
type and use the C ABI. Multi-return calls between lowC functions use an
internal caller-provided result buffer and are not directly callable through
the C ABI.

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

Function parameters and local variables share one function scope; braces do not
create nested scopes, so a name cannot be redeclared in another block of the
same function. Calls evaluate arguments from right to left. The current lowC
backend supports at most six parameters per function.

Every normal control-flow path in a lowC function definition must end in an
explicit `return`. This includes `return;` in `void` functions. An `if`
statement only guarantees a return when both branches do; loops are
conservatively treated as able to fall through. Foreign function declarations
have no body and are not subject to this rule.

### Entrypoint

Every executable program must define exactly one `entrypoint` function.

`entrypoint` always returns `void` and takes no parameters.

```c
void entrypoint(void) {
    // Program execution begins here.
    return;
}
```

Unlike C, lowC does not use `main()` as the program entrypoint.

### Calling C functions

Use `foreign c` to declare a fixed C-ABI-compatible signature for the call.
For example, the C library's `puts` accepts one string pointer and prints a
newline:

```c
foreign c s4 puts(u1 *text); // s4 matches C's 4-byte int return type.

void entrypoint(void) {
    puts("Hello from lowC"); // Calls the C library function directly.
    return; // lowC requires an explicit return, even for void functions.
}
```

Build as usual; the system C driver links the C standard library:

```sh
build/lowc -o hello hello.lowc
```

The declaration gives lowC the return and argument types and selects the C
calling convention. General variadic calls are not supported, so this is not a
general-purpose `printf` declaration.

For a complete caller and matching C implementation, see the commented
`c-example.lowc` and `helper.c` example above. Pass the `.c` file on the same
command line; lowC compiles and links it for you:

```sh
build/lowc -o hello hello.lowc helper.c
```

Prebuilt libraries can still be linked with `-L<path>` and `-l<name>`. Use
`-foreign c=clang` to select a different driver; by default lowC uses `CC` or
`cc`.

Foreign declarations currently support the x86-64 System V C ABI, fixed
integer and pointer arguments/results, and up to six parameters. Structs passed
or returned by value and automatic C-header parsing are not supported.
Generated programs currently target Linux on x86-64; the lowC function ABI,
including multi-return result buffers, is internal and is not a C ABI.

---

## Control Flow

### Conditionals

```c
s8 positive_part(s8 value) {
    if (value > 0) {
        return value; // Both branches return a value.
    } else {
        return 0;
    }
}
```

### Loops

```c
s8 sum_to(s8 limit) {
    s8 total = 0; // Initialize the accumulator.
    for (s8 i = 1; i <= limit; i = i + 1) {
        total = total + i;
    }
    return total; // Loops may run zero times.
}
```

The language currently has no `while`, `switch`, `break`, or `continue`
statements.

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
// Import only the name this file needs.
import math.lowc : add;
```

For example:

**math.lowc**

```c
s8 add(s8 a, s8 b) {
    return a + b;
}

s8 square(s8 value) {
    return value * value; // Other declarations stay private to the module.
}
```

**main.lowc**

```c
import math.lowc : add;

void entrypoint(void) {
    s8 result = add(10, 20); // `square` is not imported here.
    return;
}
```

Relative imports are resolved relative to the importing file. Standard-library
modules use the `public/` path prefix and can be imported for:

* Functions
* Structs
* Variables

The `public/` prefix is reserved for standard-library imports and never falls
back to a project-local `public/` directory. To import a local file there, use
an explicit relative path:

```c
import ./public/file.lowc : File;
```

Only the names listed by an import are visible to that source file. Declarations
in a module remain available within that module, but imports are not re-exported;
import a dependency directly to use its names from another file.

Imported files are parsed and checked in full, but unused imported functions,
globals, and structs are omitted from generated code. Imported declarations
referenced by root-source declarations, and their required dependencies, are
retained. All declarations in explicitly supplied root source files are kept.

## Standard library

### Asynchronous I/O readiness

`public/async.lowc` provides `async_poll` for waiting on multiple file
descriptors. It returns two values: the number of descriptors ready and an
error code (zero on success, otherwise a negative OS error). A timeout of
`-1` waits indefinitely; nonnegative timeouts are in milliseconds. Use the
`async_event_*` functions for event masks; combine readable, priority, and
writable masks by adding them. Error, hangup, and invalid-descriptor masks are
returned automatically. When `count` is nonzero, the descriptor pointer must
refer to an array of at least that many `AsyncPollFd` values. This is a Linux
readiness-polling primitive, not language-level `async`/`await` or thread
scheduling.

```c
import public/async.lowc : AsyncPollFd, async_poll, async_event_readable;

u1 stdin_ready(void) {
    struct AsyncPollFd descriptor = 0;
    s4 ready_count = 0;
    s4 error_code = 0;

    descriptor.descriptor = 0;
    descriptor.events = async_event_readable();
    (ready_count, error_code) = async_poll(&descriptor, 1, 0);
    return error_code == 0 && ready_count != 0;
}
```

`make install` also installs public standard-library modules beneath
`$(PREFIX)/share/lowc/public` (by default,
`/usr/local/share/lowc/public`). Its `print.lowc` module provides string
output on Linux x86-64:

```c
import public/print.lowc : print;

void entrypoint(void) {
    print("Hello, World\n");
    return;
}
```

The `args.lowc` module exposes the process command-line arguments while
preserving the required `void entrypoint(void)` signature:

```c
import public/args.lowc : args_count, args_get;
import public/print.lowc : print, print_u8;

void entrypoint(void) {
    u8 count = args_count();
    u8 index = 0;
    u1 *argument = 0;
    u1 found = 0;

    print("args count = ");
    print_u8(count);
    print("\n");

    for (index = 0; index < count; index = index + 1) {
        (found, argument) = args_get(index);
        if (found) {
            print(argument);
            print("\n");
        }
    }
    return;
}
```

`args_count()` includes the executable path, so `args_get(0)` returns
`(true, argv[0])`; subsequent indices are the arguments in order.
`args_get(index)` returns `(false, 0)` for an out-of-range index. Returned
strings are borrowed, NUL-terminated process arguments: do not modify or
deallocate them. The startup bridge currently supports Linux x86-64.

The `directory.lowc` module reads or changes the working directory and creates
or removes directories. `working_directory_get` writes a NUL-terminated path
into the supplied buffer and returns zero on success:

```c
import public/directory.lowc : working_directory_get, directory_create,
    directory_remove;

s8 directory_example(void) {
    u1 current_directory[256] = 0;
    s8 result = working_directory_get(&current_directory[0], 256);
    if (result != 0) {
        return result;
    }

    result = directory_create("lowc-example-dir", 493);
    if (result != 0) {
        return result;
    }
    return directory_remove("lowc-example-dir");
}
```

The mode `493` is decimal `0755`; the process umask may further restrict it.
Directory and working-directory system-call failures are returned as negative
OS error codes.

The `env.lowc` module gets, sets, and removes environment variables:

```c
import public/env.lowc : env_get, env_set, env_unset;
import public/string.lowc : strcmp;

s8 environment_example(void) {
    u1 found = 0;
    u1 *value = 0;
    s8 result = env_set("LOWC_README_DEMO", "ready", 1);
    if (result != 0) {
        return result;
    }

    (found, value) = env_get("LOWC_README_DEMO");
    if (!found || strcmp(value, "ready") != 0) {
        env_unset("LOWC_README_DEMO");
        return -1;
    }
    return env_unset("LOWC_README_DEMO");
}
```

`env_get` returns `(found, value)`. The returned value is borrowed from the
process environment; do not modify or deallocate it. `env_set` and `env_unset`
return zero on success or a negative error code.

String literals evaluate to `u1 *` and are stored as read-only byte data. The
public `print` function calculates their length with the public `strlen`
helper, so output stops at the first NUL byte. `print` writes to standard
output via the Linux x86-64 `write` syscall; it has no error return in this
initial API.

The recognized string escapes are `\n`, `\r`, `\t`, `\\`, and `\"`. An
unrecognized backslash escape currently emits the escaped character without
the backslash.

`print_u1`, `print_u2`, `print_u4`, and `print_u8` print unsigned integers in
decimal. `print_s1`, `print_s2`, `print_s4`, and `print_s8` do the same for
signed integers. Numeric print functions do not add a newline:

```c
import public/print.lowc : print_u8, print_s8, print;

void entrypoint(void) {
    print_u8(42);
    print_s8(-7);
    print("\n");
    return;
}
```

Use `strlen` directly by importing `string.lowc`:

```c
import public/string.lowc : strlen, parse_u8, strcmp, strcpy, strcat, strdup;
import public/memory.lowc : dealloc;

u8 string_example(void) {
    u1 *copy = strdup("Hello");
    u8 length = strlen(copy);
    dealloc(copy);
    return length;
}
```

`parse_u8(text)` returns `(success, value)`. `success` is true for a non-empty
sequence of decimal digits that fits in `u8`; on failure it returns `(false,
0)`. It accepts leading zeroes but rejects signs, whitespace, other characters,
and overflow. Destructure the result with tuple assignment:

```c
u1 parsed = 0;
u8 value = 0;
(parsed, value) = parse_u8("42");
```

`strcmp` returns a negative value, zero, or a positive value for less-than,
equal, or greater-than strings. `strcpy` and `strcat` return the destination;
the caller must provide enough space. `strdup` returns a heap-allocated copy
that should be released with `dealloc`.

The `memory.lowc` module provides Linux x86-64 `mmap`-backed allocations:

```c
import public/memory.lowc : alloc, dealloc;

u1 allocation_example(void) {
    u1 *buffer = alloc(128);
    buffer[0] = 42;
    return dealloc(buffer);
}
```

Allocation failure terminates the process with status 1. `dealloc` returns
whether the underlying unmap succeeded; only pass it pointers returned by
`alloc` or `strdup`, and release each allocation once.

The `hashmap.lowc` module provides an open-addressed `u8`-to-`u8` map with
linear probing and automatic growth. Import `HashMap`, `hashmap_init`,
`hashmap_set`, `hashmap_get`, `hashmap_remove`, and `hashmap_deinit`. Initialize
the struct before use; `hashmap_get` returns `(found, value)`, with `(false, 0)`
when the key does not exist:

```c
import public/hashmap.lowc : HashMap, hashmap_init, hashmap_set,
    hashmap_get, hashmap_remove, hashmap_deinit;

u1 hashmap_example(void) {
    struct HashMap map = 0;
    u1 found = 0;
    u8 value = 0;

    hashmap_init(&map, 8);
    hashmap_set(&map, 42, 100);
    (found, value) = hashmap_get(&map, 42);
    if (!found || value != 100) {
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

The `vector.lowc` module provides resizable arrays for all eight integer types.
This `s8` example pushes, reads, and releases a vector:

```c
import public/vector.lowc : VectorS8, vector_s8_init, vector_s8_push,
    vector_s8_get, vector_s8_deinit;

u1 vector_example(void) {
    struct VectorS8 values = 0;
    s8 value = 0;

    if (vector_s8_init(&values, 0) != 0 ||
        vector_s8_push(&values, 42) != 0 ||
        vector_s8_get(&values, 0, &value) != 0) {
        vector_s8_deinit(&values);
        return 0;
    }

    vector_s8_deinit(&values);
    return value == 42;
}
```

`vector_<type>_init` accepts an initial capacity, `push` grows as needed, and
`get`, `set`, and `remove` return a negative error for an invalid index.
Always call `vector_<type>_deinit` when finished.

The `list.lowc` module provides singly linked lists for the same integer
types. This example appends two values and reads the second:

```c
import public/list.lowc : ListS8, list_s8_init, list_s8_append,
    list_s8_get, list_s8_deinit;

u1 list_example(void) {
    struct ListS8 values = 0;
    s8 value = 0;

    if (list_s8_init(&values) != 0 ||
        list_s8_append(&values, 10) != 0 ||
        list_s8_append(&values, 20) != 0 ||
        list_s8_get(&values, 1, &value) != 0) {
        list_s8_deinit(&values);
        return 0;
    }

    list_s8_deinit(&values);
    return value == 20;
}
```

`list_<type>_remove` removes an element by index and writes its value through
an output pointer. Deinitialization releases every remaining node.

The `set.lowc` module stores unique integer values:

```c
import public/set.lowc : SetU8, set_u8_init, set_u8_add, set_u8_contains,
    set_u8_deinit;

u1 set_example(void) {
    struct SetU8 values = 0;
    u1 contains = 0;

    if (set_u8_init(&values, 0) != 0 ||
        set_u8_add(&values, 42) != 1 ||
        set_u8_contains(&values, 42, &contains) != 0) {
        set_u8_deinit(&values);
        return 0;
    }

    set_u8_deinit(&values);
    return contains;
}
```

`set_<type>_add` and `remove` return `1` when the set changes, `0` when it
does not, or a negative error. `contains` writes its answer to an output
pointer. The set implementations keep unique values and use contiguous storage.

The `file.lowc` module provides descriptor-based file operations. Opening a
file fills a `File` struct; read and write return the number of bytes
transferred, zero from `file_read` means end-of-file, and `file_close` releases
the descriptor:

```c
import public/file.lowc : File, file_open_write, file_write, file_close;

u1 write_example(void) {
    struct File file = 0;
    s8 result = file_open_write("notes.txt", &file);
    if (result != 0) {
        return 0;
    }

    result = file_write(&file, "Hello\n", 6);
    if (result != 6) {
        file_close(&file);
        return 0;
    }
    return file_close(&file) == 0;
}
```

`file_open_read` opens an existing file, `file_open_write` creates or
truncates it, and `file_open_append` creates it if needed and appends. File
operations return negative Linux `errno` values on failure; reads and writes
may be partial, so loop when the full requested length is required. Close each
successfully opened file once.

The `network.lowc` module provides hostname-resolving TCP and UDP clients. A
`TcpConnection` struct owns the connected stream socket:

```c
import public/network.lowc : TcpConnection, tcp_connect, tcp_send, tcp_receive, tcp_close;

s8 tcp_example(void) {
    struct TcpConnection connection = 0;
    s8 result = tcp_connect("example.com", 80, &connection);
    if (result != 0) {
        return result;
    }

    result = tcp_send(&connection, "GET / HTTP/1.0\r\n\r\n", 18);
    tcp_close(&connection);
    return result;
}
```

`tcp_send` and `tcp_receive` return transferred byte counts, zero on orderly
receive closure, or a negative `errno`; handle partial sends when transmitting
larger buffers. Resolver failures are returned as `-1000 - abs(EAI error)`.
Call `tcp_close` once for every successful connection.

The UDP API uses a connected datagram socket; set timeouts before receiving:

```c
import public/network.lowc : UdpConnection, udp_connect, udp_send,
    udp_receive, udp_set_timeout, udp_close;

s8 udp_example(void) {
    struct UdpConnection connection = 0;
    u1 response[64] = 0;
    s8 result = udp_connect("127.0.0.1", 9000, &connection);
    if (result != 0) {
        return result;
    }

    result = udp_set_timeout(&connection, 1000, 1000);
    if (result == 0) {
        result = udp_send(&connection, "ping", 4);
    }
    if (result >= 0) {
        result = udp_receive(&connection, &response[0], 64);
    }
    udp_close(&connection);
    return result;
}
```

`udp_send` and `udp_receive` operate on datagrams, while `tcp_set_timeout`
and `udp_set_timeout` accept receive and send timeouts in milliseconds.

The `http.lowc` module performs a basic plain-HTTP GET and writes the body to a
caller-provided buffer:

```c
import public/http.lowc : http_get;

s8 http_example(void) {
    u1 body[1024] = 0;
    s8 body_length = 0;
    s4 status_code = 0;
    (body_length, status_code) = http_get(
        "example.com", 80, "/", body, 1023
    );
    if (body_length < 0) {
        return body_length;
    }

    body[body_length] = 0;
    return status_code;
}
```

For other methods, request headers and bodies, use `http_request` directly:

```c
import public/http.lowc : HttpRequest, HttpResponse, http_request;

s8 http_post_example(void) {
    struct HttpRequest request = 0;
    struct HttpResponse response = 0;
    u1 response_body[1024] = 0;
    u1 response_headers[4096] = 0;
    s8 body_length = 0;

    request.method = "POST";
    request.path = "/submit";
    request.headers = "Content-Type: application/x-www-form-urlencoded\r\n";
    request.body = "name=lowC";
    request.body_length = 9;
    request.timeout_ms = 5000;
    response.body = &response_body[0];
    response.body_capacity = 1023;
    response.headers = &response_headers[0];
    response.headers_capacity = 4096;

    body_length = http_request("127.0.0.1", 8080, &request, &response);
    if (body_length < 0) {
        return body_length;
    }

    response_body[body_length] = 0;
    return response.status_code;
}
```

`http_get` returns `(body_length, status_code)`; `http_request` returns the
body length and writes the status and response headers to `HttpResponse`.
Request headers are CRLF-separated lines, and the request body length is in
bytes. Both APIs decode chunked response bodies. Errors include `-20001` for
malformed or incomplete responses, `-20002` when the body does not fit,
`-20003` for invalid request fields, `-20004` for a zero-byte request send,
and `-20006` when response headers exceed the provided buffer or parser
limit; network errors are propagated. The client supports plain HTTP only,
not TLS, redirects, or proxies.

The `path.lowc` module manipulates path strings without accessing the
filesystem:

```c
import public/path.lowc : path_join, path_normalize;

s8 path_example(u1 *output, u8 capacity) {
    u1 joined[256] = 0;
    s8 result = path_join("var", "logs/../app.log", &joined[0], 256);
    if (result < 0) {
        return result;
    }
    return path_normalize(&joined[0], output, capacity);
}
```

`path_join`, `path_basename`, `path_dirname`, and `path_normalize` write
NUL-terminated results to caller-owned buffers and return the byte length or a
negative error. Capacity includes the terminating NUL. `path_normalize` is
lexical; it does not resolve symlinks.

The `process.lowc` module starts an executable directly, without invoking a
shell. Its argument vector must be NUL-terminated:

```c
import public/process.lowc : Process, process_spawn, process_wait;

s8 process_example(void) {
    struct Process process = 0;
    u1 *arguments[2] = 0;
    u1 exited = 0;
    s4 exit_code = -1;
    s4 term_signal = 0;
    s8 result = 0;

    arguments[0] = "/usr/bin/true";
    result = process_spawn("/usr/bin/true", &arguments[0], &process);
    if (result != 0) {
        return result;
    }
    result = process_wait(&process, &exited, &exit_code, &term_signal);
    if (result != 0) {
        return result;
    }
    return exited && exit_code == 0 && term_signal == 0;
}
```

`process_spawn` returns after the child has successfully executed the program.
`process_wait` blocks until it exits and reports either its exit code or
terminating signal. Both functions return zero on success or a negative OS
error.

The `time.lowc` module provides monotonic time, wall-clock time, sleep, and
UTC RFC 3339 formatting:

```c
import public/time.lowc : TimeSpec, time_monotonic_ns, time_wall_clock,
    time_sleep, time_format_rfc3339_utc;
import public/print.lowc : print;

s8 time_example(void) {
    struct TimeSpec now = 0;
    u1 timestamp[31] = 0;
    s8 result = time_monotonic_ns();
    if (result < 0) {
        return result;
    }
    result = time_wall_clock(&now);
    if (result != 0) {
        return result;
    }
    result = time_sleep(0, 1000000);
    if (result != 0) {
        return result;
    }
    result = time_format_rfc3339_utc(&now, &timestamp[0], 30);
    if (result < 0) {
        return result;
    }
    timestamp[30] = 0;
    print(&timestamp[0]);
    print("\n");
    return 0;
}
```

Monotonic time is returned in nanoseconds or as a negative OS error.
`TimeSpec` stores seconds and nanoseconds; wall-clock and sleep calls return
zero or a negative error. Formatting writes 30 bytes without a terminator, so
reserve and append the 31st byte as shown.

`public/...` imports resolve beneath the standard-library root configured at
build time, normally `$(PREFIX)/share/lowc` (by default
`/usr/local/share/lowc`). Set `LOWC_STDLIB_DIR` at runtime to use a different
root without rebuilding the compiler. `make install` copies bundled standard
library files to the selected prefix and overwrites files with matching names.
To replace a bundled module with a custom version, install first, then copy
your version over the corresponding file in `share/lowc/public`.

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

Formatting cannot be combined with `--tokens`, `--ast`, `--asm`, `-debug`,
`-optimize`, `-release`, or `-o`.

## Inline assembly

Inline assembly is a statement and currently supports the x86-64 backend's
Intel-syntax assembly. Outputs precede inputs; each operand names a fixed
register, and `%0`, `%1`, and subsequent placeholders refer to outputs first,
then inputs:

```c
u8 result = 0;
asm("mov %0, %1" : "=rax"(result) : "rdi"(42) : "rcx");
```

Output constraints use `"=register"(lvalue)` and input constraints use
`"register"(expression)`. The supported 64-bit general-purpose registers are
`rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, and `r8` through `r15`. The clobber
list names registers the template modifies; `"memory"` declares memory
effects and `"cc"` declares condition-code effects. Output registers are
already treated as modified. Use `%%` for a
literal percent sign, and write empty sections explicitly (for example,
`asm("nop" : : :);`). Adjacent string literals in the assembly template are
concatenated. The compiler preserves
callee-saved registers named as operands or clobbers. `rsp` and `rbp` are
reserved and cannot be operands or clobbers; templates must not modify them.
Other code-generation targets reject inline assembly.
