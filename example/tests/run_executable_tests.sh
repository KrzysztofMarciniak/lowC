#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="${1:-$SCRIPT_DIR/../../build/lowc}"
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/lowc-tests.XXXXXX") || exit 1

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

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

void entrypoint() {
    u64 length = strlen("Hello, World\\n");
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

cat >"$TEMP_DIR/standard-library-data.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" :
    "strlen", "strcmp", "strcpy", "strcat", "strdup";
#include "$TEMP_DIR/usr/local/share/lowc/public/memory.lowc" :
    "alloc", "dealloc";
#include "$TEMP_DIR/usr/local/share/lowc/public/hashmap.lowc" :
    "HashMap", "hashmap_init", "hashmap_set", "hashmap_get",
    "hashmap_remove", "hashmap_deinit";

u64 standard_library_data_test() {
    u8 *buffer;
    u8 *copy;
    struct HashMap map;
    u64 value;
    u64 high_key;
    u64 index;

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

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
u64 invalid_output() {
    u64 result;
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
void invalid_output_lvalue() {
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
void invalid_input_register() {
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
void invalid_placeholder() {
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
echo "ok   [inline assembly] register operands, syscall, clobbers, and validation work"
