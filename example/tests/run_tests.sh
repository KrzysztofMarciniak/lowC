#!/bin/bash
#
# Run every example/tests/*.lowc file through --tokens, --ast, and
# --asm, reporting a PASS/FAIL summary and dumping output only for
# failures.
#
# Usage:
#     ./example/tests/run_tests.sh
#     ./example/tests/run_tests.sh --asm
#     ./example/tests/run_tests.sh path/to/lowc

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="$SCRIPT_DIR/../../build/lowc"
DIR="$SCRIPT_DIR"
MODES=(--tokens --ast --asm)

for arg in "$@"; do
    case "$arg" in
        --tokens|--ast|--asm)
            MODES=("$arg")
            ;;
        *)
            BIN="$arg"
            ;;
    esac
done

if [ ! -x "$BIN" ]; then
    echo "error: '$BIN' not found or not executable" >&2
    exit 1
fi

if [ ! -d "$DIR" ]; then
    echo "error: '$DIR' not found (run this from the repo root)" >&2
    exit 1
fi

total=0
failed=0

for f in "$DIR"/*.lowc; do
    name=$(basename "$f")

    for mode in "${MODES[@]}"; do
        total=$((total + 1))

        out=$("$BIN" "$mode" "$f" 2>&1)
        status=$?

        if [ $status -eq 0 ] && [ "$mode" = "--asm" ]; then
            assembler_out=$(printf '%s\n' "$out" | cc -x assembler -c -o /dev/null - 2>&1)
            status=$?
            if [ $status -ne 0 ]; then
                out="$out
$assembler_out"
            fi
        fi

        if [ $status -ne 0 ]; then
            failed=$((failed + 1))
            echo "FAIL [$mode] $name (exit $status)"
            echo "$out" | sed 's/^/    /'
            echo
        else
            echo "ok   [$mode] $name"
        fi
    done
done

if [[ " ${MODES[*]} " == *" --asm "* ]]; then
    if ! "$SCRIPT_DIR/run_executable_tests.sh" "$BIN"; then
        failed=$((failed + 1))
        total=$((total + 1))
    else
        total=$((total + 1))
    fi
fi

echo
echo "----------------------------------------"
echo "$((total - failed))/$total passed"

if [ $failed -ne 0 ]; then
    exit 1
fi
