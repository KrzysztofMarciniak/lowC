#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$TESTS_DIR/lib/test_helpers.sh"
test_init "$1" "$2" "standard_library"

if ! make -C "$REPO_ROOT" install \
    DESTDIR="$TEMP_DIR" PREFIX=/usr/local >/dev/null
then
    echo "FAIL [standard library] could not stage installed public modules"
    exit 1
fi

test_render_library_fixture "standard-library-print.lowc" || exit 1

test_copy_fixture "entrypoint_harness.c" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" "$TEMP_DIR/standard-library-print.lowc" $optimization \
        -o "$TEMP_DIR/print-$mode"
    then
        echo "FAIL [standard library] $mode print program did not build"
        exit 1
    fi

    if ! "$TEMP_DIR/print-$mode" >"$TEMP_DIR/print-$mode.out" ||
       ! printf 'Hello, World\n' | cmp -s - "$TEMP_DIR/print-$mode.out"
    then
        echo "FAIL [standard library] $mode print output was incorrect"
        exit 1
    fi
done

if ! "$BIN" --asm "$TEMP_DIR/standard-library-print.lowc" \
    >"$TEMP_DIR/print.s" ||
   ! cc -x assembler "$TEMP_DIR/print.s" -x c \
        "$TEMP_DIR/entrypoint_harness.c" -o "$TEMP_DIR/print-asm" ||
   ! "$TEMP_DIR/print-asm" >"$TEMP_DIR/print-asm.out" ||
   ! printf 'Hello, World\n' | cmp -s - "$TEMP_DIR/print-asm.out"
then
    echo "FAIL [standard library] assembly print output was incorrect"
    exit 1
fi

test_render_library_fixture "standard-library-numbers.lowc" || exit 1

test_copy_fixture "standard-library-numbers-harness.c" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization \
        "$TEMP_DIR/standard-library-numbers.lowc" \
        >"$TEMP_DIR/numbers-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/numbers-$mode.s" -x c \
            "$TEMP_DIR/standard-library-numbers-harness.c" \
            -o "$TEMP_DIR/numbers-$mode-program" ||
       ! "$TEMP_DIR/numbers-$mode-program" \
            >"$TEMP_DIR/numbers-$mode.out" ||
       ! printf '0|1|2|4|8|18446744073709551615|-1|-1|-1|-1|-9223372036854775808|9223372036854775807|done\n' \
            | cmp -s - "$TEMP_DIR/numbers-$mode.out"
    then
        echo "FAIL [standard library] $mode numeric printing or parsing was incorrect"
        cat "$TEMP_DIR/numbers-$mode.out"
        exit 1
    fi
done

test_render_library_fixture "standard-library-data.lowc" || exit 1

test_copy_fixture "standard-library-data-harness.c" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization "$TEMP_DIR/standard-library-data.lowc" \
        >"$TEMP_DIR/standard-library-data-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/standard-library-data-$mode.s" -x c \
            "$TEMP_DIR/standard-library-data-harness.c" \
            -o "$TEMP_DIR/standard-library-data-$mode" ||
       ! "$TEMP_DIR/standard-library-data-$mode"
    then
        echo "FAIL [standard library] $mode strings, heap, or hashmap behavior was incorrect"
        exit 1
    fi
done

test_render_library_fixture "standard-library-args.lowc" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" "$TEMP_DIR/standard-library-args.lowc" $optimization \
        -o "$TEMP_DIR/args-$mode" ||
       ! "$TEMP_DIR/args-$mode" alpha "two words" \
            >"$TEMP_DIR/args-$mode.out" ||
       ! printf '3|%s|alpha|two words|unchanged\n' "$TEMP_DIR/args-$mode" \
            | cmp -s - "$TEMP_DIR/args-$mode.out"
    then
        echo "FAIL [standard library] $mode command-line argument access was incorrect"
        cat "$TEMP_DIR/args-$mode.out"
        exit 1
    fi
done

test_render_library_fixture "standard-library-file.lowc" || exit 1
test_copy_fixture "standard-library-file-harness.c" || exit 1

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization \
        "$TEMP_DIR/standard-library-file.lowc" \
        >"$TEMP_DIR/file-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/file-$mode.s" -x c \
            "$TEMP_DIR/standard-library-file-harness.c" \
            -o "$TEMP_DIR/file-$mode" ||
       ! "$TEMP_DIR/file-$mode" "$TEMP_DIR/file-$mode.data"
    then
        echo "FAIL [standard library] $mode file operations were incorrect"
        exit 1
    fi
done

test_render_library_fixture "standard-library-network-http.lowc" || exit 1
test_copy_fixture "standard-library-network-http-harness.c" || exit 1
test_copy_fixture "standard-library-http-server.c" || exit 1

if ! cc -std=c11 -Wall -Wextra -Wpedantic \
    "$TEMP_DIR/standard-library-http-server.c" \
    -o "$TEMP_DIR/http-test-server"
then
    echo "FAIL [standard library] could not build local HTTP test server"
    exit 1
fi

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization \
        "$TEMP_DIR/standard-library-network-http.lowc" \
        >"$TEMP_DIR/network-http-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/network-http-$mode.s" -x c \
            "$TEMP_DIR/standard-library-network-http-harness.c" \
            -o "$TEMP_DIR/network-http-$mode"
    then
        echo "FAIL [standard library] $mode network/HTTP program did not build"
        exit 1
    fi

    "$TEMP_DIR/http-test-server" >"$TEMP_DIR/http-test-port" 2>&1 &
    server_pid=$!
    for attempt in $(seq 1 100); do
        if [ -s "$TEMP_DIR/http-test-port" ] || ! kill -0 "$server_pid" 2>/dev/null; then
            break
        fi
        sleep 0.01
    done

    if [ ! -s "$TEMP_DIR/http-test-port" ]; then
        wait "$server_pid" || true
        echo "FAIL [standard library] local HTTP test server did not start"
        cat "$TEMP_DIR/http-test-port"
        exit 1
    fi

    if ! read -r port <"$TEMP_DIR/http-test-port" ||
       ! "$TEMP_DIR/network-http-$mode" "$port"
    then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
        echo "FAIL [standard library] $mode TCP or HTTP behavior was incorrect"
        exit 1
    fi

    if ! wait "$server_pid"; then
        echo "FAIL [standard library] local HTTP test server failed"
        exit 1
    fi
done

test_pass "$TEST_GROUP" "installed standard-library behavior"
