#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/../.." && pwd)
SUPPORT_DIR="$TESTS_DIR/support"
source "$SUPPORT_DIR/test-helpers.sh"
test_init "$1" "$2" "async"

if ! make -C "$REPO_ROOT" install \
    DESTDIR="$TEMP_DIR" PREFIX=/usr/local >/dev/null
then
    echo "FAIL [async] could not stage installed public modules"
    exit 1
fi

export LOWC_STDLIB_DIR="$TEMP_DIR/usr/local/share/lowc"
test_copy_fixture "async.lowc" || exit 1
test_copy_fixture "async-harness.c" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm "$optimization" "$TEMP_DIR/async.lowc" \
        >"$TEMP_DIR/async-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/async-$mode.s" -x c \
            "$TEMP_DIR/async-harness.c" -o "$TEMP_DIR/async-$mode" ||
       ! "$TEMP_DIR/async-$mode"
    then
        echo "FAIL [async] $mode polling behavior was incorrect"
        exit 1
    fi
done

test_pass "$TEST_GROUP" "poll readiness, timeouts, error results, and invalid descriptors"
