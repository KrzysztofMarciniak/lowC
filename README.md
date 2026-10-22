# lowC

A small C-like systems language and compiler.

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

### A complete lowC program

Save this as `hello.lowc`. The comments point out several rules that differ
from the usual C entrypoint and variable conventions:

```c
foreign c s4 puts(u1 *text); // Declare a C-ABI-compatible signature for puts.

s8 add(s8 left, s8 right) {
    return left + right; // Value-returning functions return explicitly.
}

void entrypoint(void) { // lowC executables begin here, not in main().
    s8 answer = add(20, 22); // Every variable needs an initializer.
    if (answer == 42) {
        puts("The answer is 42");
    }
    return; // Even void functions explicitly return.
}
```

Compile and run it:

```sh
build/lowc -o hello hello.lowc
./hello
```

Expected output:

```text
The answer is 42
```

`puts` is provided by the C library. lowC does not parse C headers; the
`foreign c` declaration tells the compiler the return type, parameter types,
and C calling convention for this call. Pointer `const` qualifiers are not
expressed by lowC's current type syntax.

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

Build the compiler and run the compiler and generated-code checks with:

```sh
make test
make test TEST=optimization
```

The suite parses and assembles every smoke example, then runs focused test
areas. Use `TEST=smoke`, `TEST=frontend`, `TEST=modules`, `TEST=optimization`,
`TEST=semantic`, `TEST=standard_library`, or `TEST=inline_asm` to run one area.

The top-level `tests/` tree separates compiler smoke inputs, shared fixtures,
focused integration suites, and common Bash helpers.

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
#include "math.lowc" : "add";
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
#include "math.lowc" : "add";

void entrypoint(void) {
    s8 result = add(10, 20); // `square` is not imported here.
    return;
}
```

Imports are resolved relative to the importing file and can be used for:

* Functions
* Structs
* Variables

Only the names listed by an import are visible to that source file. Declarations
in a module remain available within that module, but imports are not re-exported;
import a dependency directly to use its names from another file.

Imported files are parsed and checked in full, but unused imported functions,
globals, and structs are omitted from generated code. Imported declarations
referenced by root-source declarations, and their required dependencies, are
retained. All declarations in explicitly supplied root source files are kept.

## Standard library

`make install` also installs public standard-library modules beneath
`$(PREFIX)/share/lowc/public` (by default,
`/usr/local/share/lowc/public`). Its `print.lowc` module provides string
output on Linux x86-64:

```c
#include "/usr/local/share/lowc/public/print.lowc" : "print";

void entrypoint(void) {
    print("Hello, World\n");
    return;
}
```

The `args.lowc` module exposes the process command-line arguments while
preserving the required `void entrypoint(void)` signature:

```c
#include "/usr/local/share/lowc/public/args.lowc" :
    "args_count", "args_get";
#include "/usr/local/share/lowc/public/print.lowc" :
    "print", "print_u8";

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
#include "/usr/local/share/lowc/public/print.lowc" :
    "print_u8", "print_s8", "print";

void entrypoint(void) {
    print_u8(42);
    print_s8(-7);
    print("\n");
    return;
}
```

Use `strlen` directly by importing `string.lowc`:

```c
#include "/usr/local/share/lowc/public/string.lowc" :
    "strlen", "parse_u8", "strcmp", "strcpy", "strcat", "strdup";
#include "/usr/local/share/lowc/public/memory.lowc" : "dealloc";

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
#include "/usr/local/share/lowc/public/memory.lowc" : "alloc", "dealloc";

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
#include "/usr/local/share/lowc/public/hashmap.lowc" :
    "HashMap", "hashmap_init", "hashmap_set",
    "hashmap_get", "hashmap_remove", "hashmap_deinit";

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

The `file.lowc` module provides descriptor-based file operations. Opening a
file fills a `File` struct; read and write return the number of bytes
transferred, zero from `file_read` means end-of-file, and `file_close` releases
the descriptor:

```c
#include "/usr/local/share/lowc/public/file.lowc" :
    "File", "file_open_write", "file_write", "file_close";

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

The `network.lowc` module provides a hostname-resolving TCP client. A
`TcpConnection` struct owns the connected socket:

```c
#include "/usr/local/share/lowc/public/network.lowc" :
    "TcpConnection", "tcp_connect", "tcp_send", "tcp_receive", "tcp_close";

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

The `http.lowc` module performs a basic plain-HTTP GET and writes the body to a
caller-provided buffer:

```c
#include "/usr/local/share/lowc/public/http.lowc" : "http_get";

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

On success `http_get` returns `(body_length, status_code)`. On error the first
value is negative and the status code is zero: `-20001` for malformed or
incomplete headers, `-20002` when the body does not fit, `-20003` for an
invalid host/path, `-20004` for a zero-byte request send, and `-20005` for
chunked responses (which are not decoded). Network errors are propagated.
This initial client uses plain HTTP only; it does not support TLS, redirects,
proxies, or chunked transfer encoding.

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
