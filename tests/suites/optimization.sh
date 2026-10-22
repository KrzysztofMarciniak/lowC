#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$TESTS_DIR/lib/test_helpers.sh"
test_init "$1" "$2" "optimization"

"$BIN" --asm -debug "$EXAMPLE_DIR/23_runtime.lowc" >"$TEMP_DIR/unoptimized.s" || exit 1
"$BIN" --asm -optimize "$EXAMPLE_DIR/23_runtime.lowc" >"$TEMP_DIR/optimized.s" || exit 1
"$BIN" --asm "$EXAMPLE_DIR/23_runtime.lowc" >"$TEMP_DIR/default.s" || exit 1
"$BIN" --asm -debug -optimize "$EXAMPLE_DIR/23_runtime.lowc" \
    >"$TEMP_DIR/debug-optimized.s" || exit 1

unoptimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/unoptimized.s" || true)
optimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/optimized.s" || true)
default_pushes=$(grep -c 'push rax' "$TEMP_DIR/default.s" || true)
debug_optimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/debug-optimized.s" || true)

if [ "$optimized_pushes" -ge "$unoptimized_pushes" ]; then
    echo "FAIL [optimization] optimized assembly did not reduce stack traffic"
    exit 1
fi
if [ "$default_pushes" -ne "$optimized_pushes" ]; then
    echo "FAIL [optimization] optimization was not enabled by default"
    exit 1
fi
if [ "$debug_optimized_pushes" -ne "$optimized_pushes" ]; then
    echo "FAIL [optimization] -debug -optimize did not keep optimization enabled"
    exit 1
fi

test_copy_fixture "missing-return-dead-branch.lowc" || exit 1

for mode in -debug -optimize; do
    test_expect_diagnostic \
        "require explicit return under $mode despite unreachable return" \
        "function can reach its end without an explicit return" \
        "$TEMP_DIR/missing-return-$mode.out" \
        "$BIN" "$mode" "$TEMP_DIR/missing-return-dead-branch.lowc" ||
        exit 1
done

test_copy_fixture "constant-branch.lowc" || exit 1

if ! "$BIN" --asm -debug "$TEMP_DIR/constant-branch.lowc" \
    >"$TEMP_DIR/constant-branch-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$TEMP_DIR/constant-branch.lowc" \
    >"$TEMP_DIR/constant-branch-optimized.s" ||
   ! grep -q 'jne .L' "$TEMP_DIR/constant-branch-unoptimized.s" ||
   grep -q 'jne .L' "$TEMP_DIR/constant-branch-optimized.s"
then
    echo "FAIL [optimization] constant branch was not simplified"
    exit 1
fi

if ! "$BIN" --asm -debug "$EXAMPLE_DIR/test_dead_code.lowc" \
    >"$TEMP_DIR/dead-code-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$EXAMPLE_DIR/test_dead_code.lowc" \
    >"$TEMP_DIR/dead-code-optimized.s" ||
   ! grep -q 'call dead_effect' "$TEMP_DIR/dead-code-unoptimized.s" ||
   grep -q 'call dead_effect' "$TEMP_DIR/dead-code-optimized.s" ||
   ! grep -q 'call live_effect' "$TEMP_DIR/dead-code-optimized.s"
then
    echo "FAIL [optimization] unreachable effectful block was not removed safely"
    exit 1
fi

for mode in unoptimized optimized; do
    if ! cc -x assembler "$TEMP_DIR/dead-code-$mode.s" -x c \
        "$FIXTURE_DIR/harnesses/test_dead_code_harness.c" \
        -o "$TEMP_DIR/dead-code-$mode" ||
       ! "$TEMP_DIR/dead-code-$mode"
    then
        echo "FAIL [optimization] $mode dead-code fixture changed runtime behavior"
        exit 1
    fi
done

test_copy_fixture "pure-expression.lowc" || exit 1

if ! "$BIN" --asm -debug "$TEMP_DIR/pure-expression.lowc" \
    >"$TEMP_DIR/pure-expression-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$TEMP_DIR/pure-expression.lowc" \
    >"$TEMP_DIR/pure-expression-optimized.s" ||
   ! grep -q 'push rax' "$TEMP_DIR/pure-expression-unoptimized.s" ||
   grep -q 'push rax' "$TEMP_DIR/pure-expression-optimized.s"
then
    echo "FAIL [optimization] unused pure expression was not removed"
    exit 1
fi

test_copy_fixture "trapping-expression.lowc" || exit 1

if ! "$BIN" --asm -optimize "$TEMP_DIR/trapping-expression.lowc" \
    >"$TEMP_DIR/trapping-expression-optimized.s" ||
   ! grep -q 'idiv rcx' "$TEMP_DIR/trapping-expression-optimized.s"
then
    echo "FAIL [optimization] potentially trapping expression was removed"
    exit 1
fi

test_copy_fixture "constant-call.lowc" || exit 1

test_copy_fixture "constant-call-harness.c" || exit 1

if ! "$BIN" --asm -debug "$TEMP_DIR/constant-call.lowc" \
    >"$TEMP_DIR/constant-call-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$TEMP_DIR/constant-call.lowc" \
    >"$TEMP_DIR/constant-call-optimized.s" ||
   ! grep -q 'call minushundred' "$TEMP_DIR/constant-call-unoptimized.s" ||
   grep -q 'call minushundred' "$TEMP_DIR/constant-call-optimized.s"
then
    echo "FAIL [optimization] pure constant multi-return call was not precomputed"
    exit 1
fi

for mode in unoptimized optimized; do
    if ! cc -x assembler "$TEMP_DIR/constant-call-$mode.s" -x c \
        "$TEMP_DIR/constant-call-harness.c" \
        -o "$TEMP_DIR/constant-call-$mode" ||
       ! "$TEMP_DIR/constant-call-$mode"
    then
        echo "FAIL [optimization] $mode constant-call result changed program behavior"
        exit 1
    fi
done

if ! "$BIN" -debug -optimize "$TEMP_DIR/constant-call.lowc" \
    -o "$TEMP_DIR/constant-call-release-debug" ||
   nm -a "$TEMP_DIR/constant-call-release-debug" 2>/dev/null \
       | grep -q ' minushundred$'
then
    echo "FAIL [optimization] release linker retained the precomputed function"
    exit 1
fi

if ! "$BIN" -debug "$TEMP_DIR/constant-call.lowc" \
    -o "$TEMP_DIR/constant-call-debug"
then
    echo "FAIL [optimization] could not build the unoptimized size fixture"
    exit 1
fi

unoptimized_text_size=$(
    size "$TEMP_DIR/constant-call-debug" | awk 'NR == 2 { print $1 }'
)
optimized_text_size=$(
    size "$TEMP_DIR/constant-call-release-debug" |
        awk 'NR == 2 { print $1 }'
)
if [ -z "$unoptimized_text_size" ] ||
   [ -z "$optimized_text_size" ] ||
   [ "$optimized_text_size" -ge "$unoptimized_text_size" ]
then
    echo "FAIL [optimization] precomputed release did not reduce executable text size"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/optimized.s" -x c \
    "$FIXTURE_DIR/harnesses/runtime_harness.c" -o "$TEMP_DIR/harness"
then
    echo "FAIL [runtime] could not link generated assembly with ABI harness"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/unoptimized.s" -x c \
    "$FIXTURE_DIR/harnesses/runtime_harness.c" \
    -o "$TEMP_DIR/harness-unoptimized"
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

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization \
        "$EXAMPLE_DIR/27_multiple_returns.lowc" \
        >"$TEMP_DIR/multiple-returns-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/multiple-returns-$mode.s" -x c \
           "$FIXTURE_DIR/harnesses/multiple_returns_harness.c" \
           -o "$TEMP_DIR/multiple-returns-$mode" ||
       ! "$TEMP_DIR/multiple-returns-$mode"
    then
        echo "FAIL [multiple returns] $mode values were not returned and destructured correctly"
        exit 1
    fi
done

test_pass "$TEST_GROUP" "optimization modes, dead code, and runtime"
