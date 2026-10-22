#!/bin/bash

test_init() {
    if [ "$#" -ne 3 ]; then
        echo "error: test_init expects compiler, temp root, and suite name" >&2
        exit 2
    fi

    BIN=$1
    TEST_ROOT=$2
    TEST_GROUP=$3
    TEMP_DIR="$TEST_ROOT/$TEST_GROUP"
    TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
    REPO_ROOT=$(cd -- "$TESTS_DIR/.." && pwd)
    EXAMPLE_DIR="$TESTS_DIR/examples"
    FIXTURE_DIR="$TESTS_DIR/fixtures"

    if ! mkdir -p "$TEMP_DIR"; then
        echo "error: could not create test directory '$TEMP_DIR'" >&2
        exit 1
    fi
}

test_pass() {
    printf 'ok   [%s] %s\n' "$1" "$2"
}

test_copy_fixture() {
    local fixture=$1
    local destination=${2:-$1}
    local source="$FIXTURE_DIR/$TEST_GROUP/$fixture"

    if ! cp "$source" "$TEMP_DIR/$destination"; then
        test_fail "$TEST_GROUP" "could not copy fixture '$source'"
        return 1
    fi
}

test_render_library_fixture() {
    local fixture=$1

    test_render_fixture \
        "$fixture" \
        "$TEMP_DIR/$fixture" \
        '@PUBLIC_ROOT@' \
        "$TEMP_DIR/usr/local/share/lowc/public"
}

test_render_fixture() {
    local fixture=$1
    local destination=$2
    local token=$3
    local replacement=$4
    local source="$FIXTURE_DIR/$TEST_GROUP/$fixture"
    local line

    if [ ! -f "$source" ]; then
        test_fail "$TEST_GROUP" "missing fixture '$source'"
        return 1
    fi

    while IFS= read -r line || [ -n "$line" ]; do
        line=${line//"$token"/"$replacement"}
        printf '%s\n' "$line"
    done <"$source" >"$destination"
}

test_fail() {
    local suite=$1
    local label=$2
    local output_file=
    local argument

    shift 2
    if [ "$#" -gt 0 ]; then
        output_file=$1
        shift
    fi

    printf 'FAIL [%s] %s\n' "$suite" "$label" >&2

    if [ "$#" -gt 0 ]; then
        printf '    command:' >&2
        for argument in "$@"; do
            printf ' %q' "$argument" >&2
        done
        printf '\n' >&2
    fi

    if [ -n "$output_file" ] && [ -s "$output_file" ]; then
        sed 's/^/    /' "$output_file" >&2
    fi

    return 1
}

test_expect_success() {
    local label=$1
    local output_file=$2
    shift 2

    if ! "$@" >"$output_file" 2>&1; then
        test_fail "$TEST_GROUP" "$label failed" "$output_file" "$@"
        return 1
    fi

    test_pass "$TEST_GROUP" "$label"
}

test_expect_diagnostic() {
    local label=$1
    local expected=$2
    local output_file=$3
    shift 3

    if "$@" >"$output_file" 2>&1; then
        test_fail "$TEST_GROUP" "$label unexpectedly succeeded" \
            "$output_file" "$@"
        return 1
    fi

    if ! grep -Fq -- "$expected" "$output_file"; then
        printf '    expected diagnostic: %s\n' "$expected" >&2
        test_fail "$TEST_GROUP" "$label failed for an unexpected reason" \
            "$output_file" "$@"
        return 1
    fi

    test_pass "$TEST_GROUP" "$label"
}
