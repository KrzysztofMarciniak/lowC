#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../../.." && pwd)
BIN=${1:-"$REPO_ROOT/build/lowc"}
TEST_ROOT=${2:-"$REPO_ROOT/build/networking-tests"}
TEST_DIR="$TEST_ROOT/networking"

mkdir -p "$TEST_DIR" || exit 1
if ! make -C "$REPO_ROOT" install \
    DESTDIR="$TEST_DIR/stage" PREFIX=/usr/local >/dev/null
then
    echo "FAIL [networking] could not stage installed public modules"
    exit 1
fi

export LOWC_STDLIB_DIR="$TEST_DIR/stage/usr/local/share/lowc"

for mode in debug optimize; do
    if [ "$mode" = optimize ]; then
        option=-optimize
    else
        option=-debug
    fi

    if ! "$BIN" --asm "$option" "$SCRIPT_DIR/fixtures/networking.lowc" \
        >"$TEST_DIR/networking-$mode.s" ||
       ! cc -x assembler "$TEST_DIR/networking-$mode.s" -x c \
            "$SCRIPT_DIR/fixtures/server.c" -D_DEFAULT_SOURCE -pthread \
            -o "$TEST_DIR/networking-$mode"
    then
        echo "FAIL [networking] $mode client did not build"
        exit 1
    fi

    if ! "$TEST_DIR/networking-$mode"; then
        echo "FAIL [networking] $mode local socket behavior was incorrect"
        exit 1
    fi
    echo "ok   [networking] $mode UDP, TCP timeout, HTTP, and chunk decoding"
done
