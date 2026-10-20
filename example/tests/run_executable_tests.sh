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

echo "ok   [modules] imported functions, structs, and globals assemble, link, and run"
echo "ok   [modules] unimported functions, structs, and globals are rejected"
echo "ok   [executable] optimized and unoptimized output assembles, links, and runs"
echo "ok   [standard library] installed print module writes exact output"
