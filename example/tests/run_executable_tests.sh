#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="${1:-$SCRIPT_DIR/../../build/lowc}"
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/lowc-tests.XXXXXX") || exit 1

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

cat >"$TEMP_DIR/empty-parameters.lowc" <<'EOF'
void missing_marker() {
    return;
}
EOF

if "$BIN" --ast "$TEMP_DIR/empty-parameters.lowc" \
    >"$TEMP_DIR/empty-parameters.out" 2>&1
then
    echo "FAIL [parser] accepted a function definition with empty parameters"
    exit 1
fi

if ! grep -q "functions with no parameters must use '(void)'" \
    "$TEMP_DIR/empty-parameters.out"
then
    echo "FAIL [parser] missing '(void)' diagnostic"
    cat "$TEMP_DIR/empty-parameters.out"
    exit 1
fi

cat >"$TEMP_DIR/void-parameters.lowc" <<'EOF'
void no_arguments(void) {
    return;
}

void takes_void_pointer(void *pointer) {
    return;
}

void entrypoint(void) {
    no_arguments();
    return;
}
EOF

if ! "$BIN" "$TEMP_DIR/void-parameters.lowc" \
    -o "$TEMP_DIR/void-parameters" ||
   ! "$TEMP_DIR/void-parameters"
then
    echo "FAIL [parser] explicit void parameter lists were not accepted"
    exit 1
fi

for invalid_type in u9 s9 u16 s64; do
    cat >"$TEMP_DIR/oversized-type.lowc" <<EOF
void oversized_type(void) {
    $invalid_type value;
    return;
}
EOF

    if "$BIN" --asm "$TEMP_DIR/oversized-type.lowc" \
        >"$TEMP_DIR/oversized-type.out" 2>&1
    then
        echo "FAIL [types] accepted oversized or removed type '$invalid_type'"
        exit 1
    fi
done

if ! "$BIN" --asm "$SCRIPT_DIR/19_types.lowc" \
    >"$TEMP_DIR/type-widths.s" ||
   ! grep -q "BYTE PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "WORD PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "DWORD PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "QWORD PTR" "$TEMP_DIR/type-widths.s"
then
    echo "FAIL [types] byte widths did not generate the expected x86-64 stores"
    exit 1
fi

cat >"$TEMP_DIR/type-widths-harness.c" <<'EOF'
unsigned long test_widths(void);

int
main(void)
{
    return test_widths() == 15 ? 0 : 1;
}
EOF

if ! cc -x assembler "$TEMP_DIR/type-widths.s" -x c \
    "$TEMP_DIR/type-widths-harness.c" -o "$TEMP_DIR/type-widths" ||
   ! "$TEMP_DIR/type-widths"
then
    echo "FAIL [types] byte-width arrays did not execute correctly"
    exit 1
fi

cat >"$TEMP_DIR/foreign-fixture.c" <<'EOF'
#include <stdint.h>

uint64_t lowc_foreign_add(uint64_t left, uint64_t right)
{
    return left + right;
}

uint64_t lowc_foreign_length(const unsigned char *text)
{
    uint64_t length = 0;

    while (text[length] != 0)
        length++;

    return length;
}

const unsigned char *lowc_foreign_identity(const unsigned char *text)
{
    return text;
}

uint8_t lowc_foreign_unsigned_byte(void)
{
    return 200;
}

int8_t lowc_foreign_signed_byte(void)
{
    return -7;
}
EOF

if ! cc -c "$TEMP_DIR/foreign-fixture.c" \
    -o "$TEMP_DIR/foreign-fixture.o" ||
   ! ar rcs "$TEMP_DIR/liblowc_foreign_fixture.a" \
        "$TEMP_DIR/foreign-fixture.o"
then
    echo "FAIL [foreign C] could not build fixture library"
    exit 1
fi

cat >"$TEMP_DIR/foreign-caller.lowc" <<'EOF'
foreign c u8 lowc_foreign_add(u8 left, u8 right);
foreign c u8 lowc_foreign_length(u1 *text);
foreign c u1 *lowc_foreign_identity(u1 *text);
foreign c u1 lowc_foreign_unsigned_byte(void);
foreign c s1 lowc_foreign_signed_byte(void);
foreign c s4 puts(u1 *text);

void entrypoint(void) {
    if (
        lowc_foreign_add(20, 22) == 42 &&
        lowc_foreign_length(
            lowc_foreign_identity("C ABI")
        ) == 5 &&
        lowc_foreign_unsigned_byte() == 200 &&
        lowc_foreign_signed_byte() == -7
    ) {
        puts("C interoperability works");
    }
    return;
}
EOF

if ! CC=lowc-invalid-cc "$BIN" \
    -foreign c=cc \
    "-L$TEMP_DIR" \
    -llowc_foreign_fixture \
    -o "$TEMP_DIR/foreign-caller" \
    "$TEMP_DIR/foreign-caller.lowc" ||
   ! "$TEMP_DIR/foreign-caller" >"$TEMP_DIR/foreign-caller.out" ||
   ! printf 'C interoperability works\n' \
        | cmp -s - "$TEMP_DIR/foreign-caller.out"
then
    echo "FAIL [foreign C] C library calls or linker driver selection failed"
    cat "$TEMP_DIR/foreign-caller.out"
    exit 1
fi

cat >"$TEMP_DIR/direct-helper.c" <<'EOF'
#include <stdint.h>

uint64_t lowc_direct_add(uint64_t left, uint64_t right)
{
    return left + right;
}
EOF

cat >"$TEMP_DIR/direct-caller.lowc" <<'EOF'
foreign c u8 lowc_direct_add(u8 left, u8 right);
foreign c s4 puts(u1 *text);

void entrypoint(void) {
    if (lowc_direct_add(20, 22) == 42) {
        puts("Direct C source works");
    }
    return;
}
EOF

if ! "$BIN" \
    "$TEMP_DIR/direct-helper.c" \
    "$TEMP_DIR/direct-caller.lowc" \
    -o "$TEMP_DIR/direct-caller" ||
   ! "$TEMP_DIR/direct-caller" >"$TEMP_DIR/direct-caller.out" ||
   ! printf 'Direct C source works\n' \
        | cmp -s - "$TEMP_DIR/direct-caller.out"
then
    echo "FAIL [foreign C] direct C source compilation failed"
    cat "$TEMP_DIR/direct-caller.out"
    exit 1
fi

if "$BIN" --asm \
    "$TEMP_DIR/direct-caller.lowc" \
    "$TEMP_DIR/direct-helper.c" \
    >"$TEMP_DIR/direct-asm.out" 2>&1
then
    echo "FAIL [foreign C] C source was accepted with --asm"
    exit 1
fi

cat >"$TEMP_DIR/unsupported-foreign.lowc" <<'EOF'
foreign forth u8 unsupported(void);
void entrypoint(void) {
    return;
}
EOF

if "$BIN" --asm "$TEMP_DIR/unsupported-foreign.lowc" \
    >"$TEMP_DIR/unsupported-foreign.out" 2>&1
then
    echo "FAIL [foreign C] unsupported foreign ABI was accepted"
    exit 1
fi

cat >"$TEMP_DIR/format-input.lowc" <<'EOF'
#include "z.lowc" : "z";
#include "a.lowc" : "a";
void entrypoint(void){u8 b=2;u8 a=1;if(a){b=b+1;long_name=2;}}
EOF
cp "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-original.lowc"

if ! "$BIN" --format "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-stdout"
then
    echo "FAIL [format] fixed-style formatter did not run"
    exit 1
fi

cat >"$TEMP_DIR/format-expected.lowc" <<'EOF'
#include "a.lowc" : "a";
#include "z.lowc" : "z";

void entrypoint(void) {
        u8 b = 2;
        u8 a = 1;
        if (a) {
                b         = b + 1;
                long_name = 2;
        }
}
EOF

if [ -s "$TEMP_DIR/format-stdout" ] ||
   ! cmp -s "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-expected.lowc" ||
   cmp -s "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-original.lowc"
then
    echo "FAIL [format] source was not overwritten with the fixed style"
    cat "$TEMP_DIR/format-input.lowc"
    exit 1
fi

cp "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-before-roundtrip.lowc"
if ! "$BIN" --format "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-roundtrip-stdout" ||
   [ -s "$TEMP_DIR/format-roundtrip-stdout" ] ||
   ! cmp -s "$TEMP_DIR/format-input.lowc" \
        "$TEMP_DIR/format-before-roundtrip.lowc"
then
    echo "FAIL [format] formatting was not idempotent"
    exit 1
fi

if "$BIN" --format --asm "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-conflict.out" 2>&1
then
    echo "FAIL [format] conflicting output modes were accepted"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/23_runtime.lowc" \
    -O1 -o "$TEMP_DIR/program with space"
then
    echo "FAIL [executable] compiler invocation"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/23_runtime.lowc" \
    -o "$TEMP_DIR/program-unoptimized"
then
    echo "FAIL [executable] unoptimized compiler invocation"
    exit 1
fi

if ! "$TEMP_DIR/program with space"; then
    echo "FAIL [executable] optimized program did not run successfully"
    exit 1
fi

if ! "$TEMP_DIR/program-unoptimized"; then
    echo "FAIL [executable] unoptimized program did not run successfully"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/21_modules.lowc" -o "$TEMP_DIR/module-program" ||
   ! "$TEMP_DIR/module-program"
then
    echo "FAIL [modules] imported executable did not build and run"
    exit 1
fi

if ! "$BIN" --asm "$SCRIPT_DIR/24_modules.lowc" >"$TEMP_DIR/modules.s"; then
    echo "FAIL [modules] could not generate assembly for imported declarations"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/modules.s" -x c \
    "$SCRIPT_DIR/module_harness.c" -o "$TEMP_DIR/modules-harness"
then
    echo "FAIL [modules] imported declarations did not assemble and link"
    exit 1
fi

if ! "$TEMP_DIR/modules-harness"; then
    echo "FAIL [modules] imported functions, structs, or globals returned incorrect results"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/24_modules.lowc" -o "$TEMP_DIR/modules-program" ||
   ! "$TEMP_DIR/modules-program"
then
    echo "FAIL [modules] imported declarations did not build and run as an executable"
    exit 1
fi

MODULE_ACCESS_DIR="$SCRIPT_DIR/module_access"

if ! "$BIN" --asm "$MODULE_ACCESS_DIR/visibility_main.lowc" \
    >"$TEMP_DIR/visibility.s"
then
    echo "FAIL [modules] selected declarations were not visible"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/visibility.s" -x c \
    "$MODULE_ACCESS_DIR/harness.c" -o "$TEMP_DIR/visibility-harness" ||
   ! "$TEMP_DIR/visibility-harness"
then
    echo "FAIL [modules] selected declarations or private helpers returned incorrect results"
    exit 1
fi

if ! "$BIN" "$MODULE_ACCESS_DIR/visibility_main.lowc" \
    -o "$TEMP_DIR/visibility-program" ||
   ! "$TEMP_DIR/visibility-program"
then
    echo "FAIL [modules] visibility example did not build and run"
    exit 1
fi

for hidden_case in function global struct dependency; do
    if "$BIN" --asm "$MODULE_ACCESS_DIR/hidden_${hidden_case}.lowc" \
        >"$TEMP_DIR/hidden-${hidden_case}.out" 2>&1
    then
        echo "FAIL [modules] unimported ${hidden_case} was accessible"
        exit 1
    fi

    if ! grep -q "is not imported" \
        "$TEMP_DIR/hidden-${hidden_case}.out"
    then
        echo "FAIL [modules] ${hidden_case} visibility failed for an unrelated reason"
        cat "$TEMP_DIR/hidden-${hidden_case}.out"
        exit 1
    fi
done

"$BIN" --asm "$SCRIPT_DIR/23_runtime.lowc" >"$TEMP_DIR/unoptimized.s" || exit 1
"$BIN" --asm -O1 "$SCRIPT_DIR/23_runtime.lowc" >"$TEMP_DIR/optimized.s" || exit 1

unoptimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/unoptimized.s" || true)
optimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/optimized.s" || true)

if [ "$optimized_pushes" -ge "$unoptimized_pushes" ]; then
    echo "FAIL [optimization] optimized assembly did not reduce stack traffic"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/optimized.s" -x c \
    "$SCRIPT_DIR/runtime_harness.c" -o "$TEMP_DIR/harness"
then
    echo "FAIL [runtime] could not link generated assembly with ABI harness"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/unoptimized.s" -x c \
    "$SCRIPT_DIR/runtime_harness.c" -o "$TEMP_DIR/harness-unoptimized"
then
    echo "FAIL [runtime] could not link unoptimized assembly with ABI harness"
    exit 1
fi

if ! "$TEMP_DIR/harness"; then
    echo "FAIL [runtime] generated functions returned incorrect results"
    exit 1
fi

if ! "$TEMP_DIR/harness-unoptimized"; then
    echo "FAIL [runtime] unoptimized functions returned incorrect results"
    exit 1
fi

if ! make -C "$SCRIPT_DIR/../.." install \
    DESTDIR="$TEMP_DIR" PREFIX=/usr/local >/dev/null
then
    echo "FAIL [standard library] could not stage installed public modules"
    exit 1
fi

cat >"$TEMP_DIR/standard-library-print.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/print.lowc" : "print";
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" : "strlen";

void entrypoint(void) {
    u8 length = strlen("Hello, World\\n");
    print("Hello, World\\n");
}
EOF

cat >"$TEMP_DIR/entrypoint_harness.c" <<'EOF'
void entrypoint(void);

int
main(void)
{
    entrypoint();
    return 0;
}
EOF

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-O1
    else
        optimization=
    fi

    if ! "$BIN" "$TEMP_DIR/standard-library-print.lowc" $optimization \
        -o "$TEMP_DIR/print-$mode"
    then
        echo "FAIL [standard library] $mode print program did not build"
        exit 1
    fi

    if ! "$TEMP_DIR/print-$mode" >"$TEMP_DIR/print-$mode.out" ||
       ! printf 'Hello, World\n' | cmp -s - "$TEMP_DIR/print-$mode.out"
    then
        echo "FAIL [standard library] $mode print output was incorrect"
        exit 1
    fi
done

if ! "$BIN" --asm "$TEMP_DIR/standard-library-print.lowc" \
    >"$TEMP_DIR/print.s" ||
   ! cc -x assembler "$TEMP_DIR/print.s" -x c \
        "$TEMP_DIR/entrypoint_harness.c" -o "$TEMP_DIR/print-asm" ||
   ! "$TEMP_DIR/print-asm" >"$TEMP_DIR/print-asm.out" ||
   ! printf 'Hello, World\n' | cmp -s - "$TEMP_DIR/print-asm.out"
then
    echo "FAIL [standard library] assembly print output was incorrect"
    exit 1
fi

cat >"$TEMP_DIR/standard-library-numbers.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/print.lowc" :
    "print", "print_u1", "print_u2", "print_u4", "print_u8",
    "print_s1", "print_s2", "print_s4", "print_s8";
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" : "parse_u8";

u8 standard_library_numbers_test(void) {
    u8 value;
    s8 minimum;

    if (!parse_u8("0", &value) || value != 0) {
        return 1;
    }
    print_u8(value);
    print("|");

    print_u1(1);
    print("|");
    print_u2(2);
    print("|");
    print_u4(4);
    print("|");
    print_u8(8);
    print("|");

    if (!parse_u8("18446744073709551615", &value)) {
        return 2;
    }
    print_u8(value);
    print("|");

    if (!parse_u8("00000000000000000042", &value) || value != 42) {
        return 3;
    }

    if (parse_u8("", &value) || value != 42) {
        return 4;
    }
    if (parse_u8("-1", &value) || value != 42) {
        return 5;
    }
    if (parse_u8("12x", &value) || value != 42) {
        return 6;
    }
    if (parse_u8("18446744073709551616", &value) || value != 42) {
        return 7;
    }

    print_s1(-1);
    print("|");
    print_s2(-1);
    print("|");
    print_s4(-1);
    print("|");
    print_s8(-1);
    print("|");
    asm("mov %0, 0x8000000000000000" : "=rax"(minimum) : :);
    print_s8(minimum);
    print("|");
    print_s8(9223372036854775807);
    print("|done\n");

    return 0;
}
EOF

cat >"$TEMP_DIR/standard-library-numbers-harness.c" <<'EOF'
unsigned long standard_library_numbers_test(void);

int
main(void)
{
    return standard_library_numbers_test() == 0 ? 0 : 1;
}
EOF

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-O1
    else
        optimization=
    fi

    if ! "$BIN" --asm $optimization \
        "$TEMP_DIR/standard-library-numbers.lowc" \
        >"$TEMP_DIR/numbers-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/numbers-$mode.s" -x c \
            "$TEMP_DIR/standard-library-numbers-harness.c" \
            -o "$TEMP_DIR/numbers-$mode-program" ||
       ! "$TEMP_DIR/numbers-$mode-program" \
            >"$TEMP_DIR/numbers-$mode.out" ||
       ! printf '0|1|2|4|8|18446744073709551615|-1|-1|-1|-1|-9223372036854775808|9223372036854775807|done\n' \
            | cmp -s - "$TEMP_DIR/numbers-$mode.out"
    then
        echo "FAIL [standard library] $mode numeric printing or parsing was incorrect"
        cat "$TEMP_DIR/numbers-$mode.out"
        exit 1
    fi
done

cat >"$TEMP_DIR/standard-library-data.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" :
    "strlen", "strcmp", "strcpy", "strcat", "strdup";
#include "$TEMP_DIR/usr/local/share/lowc/public/memory.lowc" :
    "alloc", "dealloc";
#include "$TEMP_DIR/usr/local/share/lowc/public/hashmap.lowc" :
    "HashMap", "hashmap_init", "hashmap_set", "hashmap_get",
    "hashmap_remove", "hashmap_deinit";

u8 standard_library_data_test(void) {
    u1 *buffer;
    u1 *copy;
    struct HashMap map;
    u8 value;
    u8 high_key;
    u8 index;

    buffer = alloc(32);
    strcpy(buffer, "low");
    strcat(buffer, "C");
    if (strcmp(buffer, "lowC") != 0 || strlen(buffer) != 4) {
        return 1;
    }

    copy = strdup(buffer);
    if (strcmp(copy, "lowC") != 0 || dealloc(copy) == 0) {
        return 2;
    }

    hashmap_init(&map, 8);
    for (index = 0; index < 12; index = index + 1) {
        hashmap_set(&map, index, index * 7);
    }

    asm("mov %0, 0x8000000000000000" : "=rax"(high_key) : :);
    hashmap_set(&map, high_key, 99);
    if (hashmap_get(&map, high_key, &value) == 0 || value != 99) {
        return 3;
    }

    if (hashmap_get(&map, 8, &value) == 0 || value != 56) {
        return 4;
    }

    if (hashmap_remove(&map, 8) == 0 || hashmap_get(&map, 8, &value) != 0) {
        return 5;
    }

    hashmap_set(&map, 24, 168);
    if (hashmap_get(&map, 24, &value) == 0 || value != 168) {
        return 6;
    }

    if (hashmap_get(&map, 9, &value) == 0 || value != 63) {
        return 7;
    }

    if (hashmap_deinit(&map) == 0 || dealloc(buffer) == 0) {
        return 8;
    }

    return 0;
}
EOF

cat >"$TEMP_DIR/standard-library-data-harness.c" <<'EOF'
unsigned long standard_library_data_test(void);

int
main(void)
{
    return standard_library_data_test() == 0 ? 0 : 1;
}
EOF

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-O1
    else
        optimization=
    fi

    if ! "$BIN" --asm $optimization "$TEMP_DIR/standard-library-data.lowc" \
        >"$TEMP_DIR/standard-library-data-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/standard-library-data-$mode.s" -x c \
            "$TEMP_DIR/standard-library-data-harness.c" \
            -o "$TEMP_DIR/standard-library-data-$mode" ||
       ! "$TEMP_DIR/standard-library-data-$mode"
    then
        echo "FAIL [standard library] $mode strings, heap, or hashmap behavior was incorrect"
        exit 1
    fi
done

if ! "$BIN" --asm "$SCRIPT_DIR/26_inline_asm.lowc" \
    >"$TEMP_DIR/inline-asm.s"
then
    echo "FAIL [inline assembly] valid assembly did not compile"
    exit 1
fi

cat >"$TEMP_DIR/inline-asm-harness.c" <<'EOF'
unsigned long inline_assembly_move(void);
unsigned long inline_assembly_syscall_pid(void);
unsigned long inline_assembly_callee_saved(void);
unsigned long inline_assembly_call(void);

int
main(void)
{
    return inline_assembly_move() == 42 &&
           inline_assembly_syscall_pid() > 0 &&
           inline_assembly_callee_saved() == 17 &&
           inline_assembly_call() == 23
        ? 0
        : 1;
}
EOF

if ! cc -x assembler "$TEMP_DIR/inline-asm.s" -x c \
    "$TEMP_DIR/inline-asm-harness.c" -o "$TEMP_DIR/inline-asm-program" ||
   ! "$TEMP_DIR/inline-asm-program"
then
    echo "FAIL [inline assembly] outputs, syscall, or callee-saved handling failed"
    exit 1
fi

cat >"$TEMP_DIR/standard-library-args.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/args.lowc" :
    "args_count", "args_get";
#include "$TEMP_DIR/usr/local/share/lowc/public/print.lowc" :
    "print", "print_u8";

void entrypoint(void) {
    u8 count;
    u1 *argument;

    count = args_count();
    print_u8(count);
    print("|");

    if (args_get(0, &argument)) {
        print(argument);
    } else {
        print("missing");
    }
    print("|");

    if (args_get(1, &argument)) {
        print(argument);
    } else {
        print("missing");
    }
    print("|");

    if (args_get(2, &argument)) {
        print(argument);
    } else {
        print("missing");
    }
    print("|");

    argument = "unchanged";
    if (args_get(count, &argument)) {
        print("unexpected");
    }
    print(argument);
    print("\\n");
}
EOF

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-O1
    else
        optimization=
    fi

    if ! "$BIN" "$TEMP_DIR/standard-library-args.lowc" $optimization \
        -o "$TEMP_DIR/args-$mode" ||
       ! "$TEMP_DIR/args-$mode" alpha "two words" \
            >"$TEMP_DIR/args-$mode.out" ||
       ! printf '3|%s|alpha|two words|unchanged\n' "$TEMP_DIR/args-$mode" \
            | cmp -s - "$TEMP_DIR/args-$mode.out"
    then
        echo "FAIL [standard library] $mode command-line argument access was incorrect"
        cat "$TEMP_DIR/args-$mode.out"
        exit 1
    fi
done

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
u8 invalid_output(void) {
    u8 result;
    asm("nop" : "=rsp"(result) : :);
    return result;
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "invalid inline assembly output register constraint" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] reserved rsp output was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
void invalid_output_lvalue(void) {
    asm("nop" : "=rax"(42) : :);
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "inline assembly output must be an assignable lvalue" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] non-lvalue output was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
void invalid_input_register(void) {
    asm("nop" : : "foo"(1) :);
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "invalid inline assembly input register constraint" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] unknown input register was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
void invalid_placeholder(void) {
    asm("mov %0, %1" : : :);
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "inline assembly operand placeholder is out of range" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] invalid placeholder was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

echo "ok   [modules] imported functions, structs, and globals assemble, link, and run"
echo "ok   [modules] unimported functions, structs, and globals are rejected"
echo "ok   [executable] optimized and unoptimized output assembles, links, and runs"
echo "ok   [standard library] installed print module writes exact output"
echo "ok   [standard library] CLI arguments include argv[0] and are bounds checked"
echo "ok   [inline assembly] register operands, syscall, clobbers, and validation work"
