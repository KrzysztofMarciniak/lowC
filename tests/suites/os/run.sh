#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/../.." && pwd)
source "$TESTS_DIR/support/test-helpers.sh"
test_init "$1" "$2" "os"

export LOWC_STDLIB_DIR="$REPO_ROOT/stdlib"

test_copy_fixture "os.lowc" || exit 1
test_copy_fixture "harness.c" || exit 1

if ! "$BIN" --asm "$TEMP_DIR/os.lowc" >"$TEMP_DIR/os.s" ||
   ! cc -x assembler "$TEMP_DIR/os.s" -x c "$TEMP_DIR/harness.c" \
       -o "$TEMP_DIR/os-test" ||
   ! "$TEMP_DIR/os-test" "$(cd -- "$TEMP_DIR" && pwd)"
then
    echo "FAIL [os] paths, environment, filesystem, cwd, or processes failed"
    exit 1
fi

test_pass "$TEST_GROUP" "OS and process utilities"
