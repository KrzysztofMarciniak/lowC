#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$TESTS_DIR/lib/test_helpers.sh"
test_init "$1" "$2" "inline_asm"

if ! "$BIN" --asm "$EXAMPLE_DIR/26_inline_asm.lowc" \
    >"$TEMP_DIR/inline-asm.s"
then
    echo "FAIL [inline assembly] valid assembly did not compile"
    exit 1
fi

test_copy_fixture "inline-asm-harness.c" || exit 1

if ! cc -x assembler "$TEMP_DIR/inline-asm.s" -x c \
    "$TEMP_DIR/inline-asm-harness.c" -o "$TEMP_DIR/inline-asm-program" ||
   ! "$TEMP_DIR/inline-asm-program"
then
    echo "FAIL [inline assembly] outputs, syscall, or callee-saved handling failed"
    exit 1
fi

test_copy_fixture "invalid-output-register.lowc" "invalid-inline-asm.lowc" || exit 1

test_expect_diagnostic \
    "reject reserved rsp output constraint" \
    "invalid inline assembly output register constraint" \
    "$TEMP_DIR/invalid-inline-asm.out" \
    "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" ||
    exit 1

test_copy_fixture "invalid-output-lvalue.lowc" "invalid-inline-asm.lowc" || exit 1

test_expect_diagnostic \
    "reject non-lvalue output operand" \
    "inline assembly output must be an assignable lvalue" \
    "$TEMP_DIR/invalid-inline-asm.out" \
    "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" ||
    exit 1

test_copy_fixture "invalid-input-register.lowc" "invalid-inline-asm.lowc" || exit 1

test_expect_diagnostic \
    "reject unknown input register" \
    "invalid inline assembly input register constraint" \
    "$TEMP_DIR/invalid-inline-asm.out" \
    "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" ||
    exit 1

test_copy_fixture "invalid-placeholder.lowc" "invalid-inline-asm.lowc" || exit 1

test_expect_diagnostic \
    "reject out-of-range operand placeholder" \
    "inline assembly operand placeholder is out of range" \
    "$TEMP_DIR/invalid-inline-asm.out" \
    "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" ||
    exit 1

test_pass "$TEST_GROUP" "inline-assembly execution and validation"
