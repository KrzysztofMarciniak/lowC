#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/../.." && pwd)
SUPPORT_DIR="$TESTS_DIR/support"
source "$SUPPORT_DIR/test-helpers.sh"
test_init "$1" "$2" "time"

export LOWC_STDLIB_DIR="$REPO_ROOT/stdlib"
test_copy_fixture "time.lowc" || exit 1
test_copy_fixture "time-harness.c" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization "$TEMP_DIR/time.lowc" \
        >"$TEMP_DIR/time-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/time-$mode.s" -x c \
            "$TEMP_DIR/time-harness.c" -o "$TEMP_DIR/time-$mode" ||
       ! "$TEMP_DIR/time-$mode"
    then
        echo "FAIL [time] $mode clock, sleep, or formatting behavior was incorrect"
        exit 1
    fi
done

test_pass "$TEST_GROUP" "Linux x86-64 time utilities"
