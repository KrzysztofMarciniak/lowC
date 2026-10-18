#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="${1:-$SCRIPT_DIR/../../build/lowc}"
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/lowc-tests.XXXXXX") || exit 1

cleanup() {
    rm -f "$TEMP_DIR/program with space" \
        "$TEMP_DIR/program-unoptimized" \
        "$TEMP_DIR/module-program" \
        "$TEMP_DIR/modules-program" \
        "$TEMP_DIR/optimized.s" \
        "$TEMP_DIR/unoptimized.s" \
        "$TEMP_DIR/harness" \
        "$TEMP_DIR/harness-unoptimized" \
        "$TEMP_DIR/modules.s" \
        "$TEMP_DIR/modules-harness"
    rmdir "$TEMP_DIR"
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

echo "ok   [modules] imported functions, structs, and globals assemble, link, and run"
echo "ok   [executable] optimized and unoptimized output assembles, links, and runs"
