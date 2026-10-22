#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$TESTS_DIR/lib/test_helpers.sh"
test_init "$1" "$2" "semantic"

test_copy_fixture "explicit-returns-valid.lowc" || exit 1

test_expect_success \
    "compile valid explicit-return paths" \
    "$TEMP_DIR/explicit-returns-valid-build.out" \
    "$BIN" "$TEMP_DIR/explicit-returns-valid.lowc" \
        -o "$TEMP_DIR/explicit-returns-valid" ||
    exit 1

if ! "$TEMP_DIR/explicit-returns-valid"; then
    test_fail "$TEST_GROUP" "valid explicit-return paths did not execute"
    exit 1
fi

test_copy_fixture "explicit-returns-missing-void.lowc" || exit 1

test_expect_diagnostic \
    "reject void function without return" \
    "without an explicit return" \
    "$TEMP_DIR/explicit-returns-missing-void.out" \
    "$BIN" --asm "$TEMP_DIR/explicit-returns-missing-void.lowc" ||
    exit 1

test_copy_fixture "explicit-returns-missing-branch.lowc" || exit 1

test_expect_diagnostic \
    "reject value function with a fallthrough branch" \
    "without an explicit return" \
    "$TEMP_DIR/explicit-returns-missing-branch.out" \
    "$BIN" --asm "$TEMP_DIR/explicit-returns-missing-branch.lowc" ||
    exit 1

test_copy_fixture "explicit-returns-loop.lowc" || exit 1

test_expect_diagnostic \
    "reject possibly skipped loop as guaranteed return" \
    "without an explicit return" \
    "$TEMP_DIR/explicit-returns-loop.out" \
    "$BIN" --asm "$TEMP_DIR/explicit-returns-loop.lowc" ||
    exit 1

test_copy_fixture "multiple-return-count.lowc" || exit 1

test_expect_diagnostic \
    "reject return with wrong value count" \
    "wrong number of values" \
    "$TEMP_DIR/multiple-return-count.out" \
    "$BIN" --asm "$TEMP_DIR/multiple-return-count.lowc" ||
    exit 1

test_copy_fixture "multiple-return-targets.lowc" || exit 1

test_expect_diagnostic \
    "reject tuple assignment with wrong target count" \
    "wrong number of targets" \
    "$TEMP_DIR/multiple-return-targets.out" \
    "$BIN" --asm "$TEMP_DIR/multiple-return-targets.lowc" ||
    exit 1

test_copy_fixture "multiple-return-type.lowc" || exit 1

test_expect_diagnostic \
    "reject tuple assignment with incompatible target type" \
    "incompatible type" \
    "$TEMP_DIR/multiple-return-type.out" \
    "$BIN" --asm "$TEMP_DIR/multiple-return-type.lowc" ||
    exit 1

test_copy_fixture "foreign-multiple-return.lowc" || exit 1

test_expect_diagnostic \
    "reject foreign function with multiple returns" \
    "foreign functions must declare exactly one return type" \
    "$TEMP_DIR/foreign-multiple-return.out" \
    "$BIN" --ast "$TEMP_DIR/foreign-multiple-return.lowc" ||
    exit 1

test_copy_fixture "too-many-lowc-parameters.lowc" || exit 1

test_expect_diagnostic \
    "reject function with too many parameters" \
    "lowC functions support at most six parameters" \
    "$TEMP_DIR/too-many-lowc-parameters.out" \
    "$BIN" --asm "$TEMP_DIR/too-many-lowc-parameters.lowc" ||
    exit 1

test_pass "$TEST_GROUP" "return rules, multiple returns, and function validation"
