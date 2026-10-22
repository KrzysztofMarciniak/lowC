#!/bin/bash
#
# Run smoke examples and a focused integration suite, optionally filtered by area.

set -uo pipefail

TESTS_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$TESTS_DIR/.." && pwd)
BIN="${1:-$REPO_ROOT/build/lowc}"
FILTER="${2:-all}"

case "$BIN" in
    /*) ;;
    *) BIN="$REPO_ROOT/$BIN" ;;
esac

if [ ! -x "$BIN" ]; then
    echo "error: '$BIN' not found or not executable" >&2
    exit 1
fi

SUITES=(frontend modules optimization semantic standard_library inline_asm)

case "$FILTER" in
    all|smoke|frontend|modules|optimization|semantic|standard_library|inline_asm)
        ;;
    *)
        echo "error: unknown test suite '$FILTER'" >&2
        echo "available suites: all smoke ${SUITES[*]}" >&2
        exit 2
        ;;
esac

TEMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/lowc-tests.XXXXXX") || exit 1

cleanup() {
    rm -rf "$TEMP_ROOT"
}
trap cleanup EXIT

total=0
failed=0

run_smoke_tests() {
    local source mode name output status assembler_output

    for source in "$TESTS_DIR"/examples/*.lowc; do
        if [ ! -f "$source" ]; then
            echo "error: no smoke examples found in '$TESTS_DIR/examples'" >&2
            failed=$((failed + 1))
            return
        fi

        name=$(basename "$source")
        for mode in --tokens --ast --asm; do
            total=$((total + 1))
            output=$("$BIN" "$mode" "$source" 2>&1)
            status=$?

            if [ "$status" -eq 0 ] && [ "$mode" = "--asm" ]; then
                assembler_output=$(
                    printf '%s\n' "$output" |
                        cc -x assembler -c -o /dev/null - 2>&1
                )
                status=$?
                if [ "$status" -ne 0 ]; then
                    output="$output
$assembler_output"
                fi
            fi

            if [ "$status" -ne 0 ]; then
                failed=$((failed + 1))
                echo "FAIL [$mode] $name (exit $status)"
                printf '%s\n' "$output" | sed 's/^/    /'
                echo
            else
                echo "ok   [$mode] $name"
            fi
        done
    done
}

run_suite() {
    local suite=$1
    total=$((total + 1))

    if ! bash "$TESTS_DIR/suites/$suite.sh" "$BIN" "$TEMP_ROOT"; then
        failed=$((failed + 1))
        echo "FAIL [suite] $suite"
    fi
}

if [ "$FILTER" = all ] || [ "$FILTER" = smoke ]; then
    run_smoke_tests
fi

if [ "$FILTER" != smoke ]; then
    for suite in "${SUITES[@]}"; do
        if [ "$FILTER" = all ] || [ "$FILTER" = "$suite" ]; then
            run_suite "$suite"
        fi
    done
fi

echo
echo "----------------------------------------"
echo "$((total - failed))/$total passed"

if [ "$failed" -ne 0 ]; then
    exit 1
fi
