#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../../.." && pwd)
BUILD_DIR="$REPO_ROOT/build/collections-test"

if [ -e "$BUILD_DIR" ]; then
    echo "FAIL [collections] build directory already exists: $BUILD_DIR" >&2
    exit 1
fi
mkdir -p "$BUILD_DIR"
trap 'rm -rf "$BUILD_DIR"' EXIT

export LOWC_STDLIB_DIR="$REPO_ROOT/stdlib"

for mode in debug optimize; do
    "$REPO_ROOT/build/lowc" --asm "-$mode" "$SCRIPT_DIR/collections.lowc" \
        >"$BUILD_DIR/collections-$mode.s"
    cc -x assembler "$BUILD_DIR/collections-$mode.s" -x c \
        "$SCRIPT_DIR/harness.c" -o "$BUILD_DIR/collections-$mode"
    "$BUILD_DIR/collections-$mode"
done
