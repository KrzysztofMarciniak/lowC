#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$TESTS_DIR/lib/test_helpers.sh"
test_init "$1" "$2" "frontend"

test_copy_fixture "empty-parameters.lowc" || exit 1

test_expect_diagnostic \
    "reject empty function parameter lists" \
    "functions with no parameters must use '(void)'" \
    "$TEMP_DIR/empty-parameters.out" \
    "$BIN" --ast "$TEMP_DIR/empty-parameters.lowc" ||
    exit 1

test_copy_fixture "void-parameters.lowc" || exit 1

if ! "$BIN" "$TEMP_DIR/void-parameters.lowc" \
    -o "$TEMP_DIR/void-parameters" ||
   ! "$TEMP_DIR/void-parameters"
then
    echo "FAIL [parser] explicit void parameter lists were not accepted"
    exit 1
fi

for invalid_type in u9 s9 u16 s64; do
    cat >"$TEMP_DIR/oversized-type.lowc" <<EOF
void oversized_type(void) {
    $invalid_type value;
    return;
}
EOF

    if "$BIN" --asm "$TEMP_DIR/oversized-type.lowc" \
        >"$TEMP_DIR/oversized-type.out" 2>&1
    then
        echo "FAIL [types] accepted oversized or removed type '$invalid_type'"
        exit 1
    fi
done

if ! "$BIN" --asm "$EXAMPLE_DIR/19_types.lowc" \
    >"$TEMP_DIR/type-widths.s" ||
   ! grep -q "BYTE PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "WORD PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "DWORD PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "QWORD PTR" "$TEMP_DIR/type-widths.s"
then
    echo "FAIL [types] byte widths did not generate the expected x86-64 stores"
    exit 1
fi

test_copy_fixture "type-widths-harness.c" || exit 1

if ! cc -x assembler "$TEMP_DIR/type-widths.s" -x c \
    "$TEMP_DIR/type-widths-harness.c" -o "$TEMP_DIR/type-widths" ||
   ! "$TEMP_DIR/type-widths"
then
    echo "FAIL [types] byte-width arrays did not execute correctly"
    exit 1
fi

test_copy_fixture "foreign-fixture.c" || exit 1

if ! cc -c "$TEMP_DIR/foreign-fixture.c" \
    -o "$TEMP_DIR/foreign-fixture.o" ||
   ! ar rcs "$TEMP_DIR/liblowc_foreign_fixture.a" \
        "$TEMP_DIR/foreign-fixture.o"
then
    echo "FAIL [foreign C] could not build fixture library"
    exit 1
fi

test_copy_fixture "foreign-caller.lowc" || exit 1

if ! CC=lowc-invalid-cc "$BIN" \
    -foreign c=cc \
    "-L$TEMP_DIR" \
    -llowc_foreign_fixture \
    -o "$TEMP_DIR/foreign-caller" \
    "$TEMP_DIR/foreign-caller.lowc" ||
   ! "$TEMP_DIR/foreign-caller" >"$TEMP_DIR/foreign-caller.out" ||
   ! printf 'C interoperability works\n' \
        | cmp -s - "$TEMP_DIR/foreign-caller.out"
then
    echo "FAIL [foreign C] C library calls or linker driver selection failed"
    cat "$TEMP_DIR/foreign-caller.out"
    exit 1
fi

test_copy_fixture "direct-helper.c" || exit 1

test_copy_fixture "direct-caller.lowc" || exit 1

if ! "$BIN" \
    "$TEMP_DIR/direct-helper.c" \
    "$TEMP_DIR/direct-caller.lowc" \
    -o "$TEMP_DIR/direct-caller" ||
   ! "$TEMP_DIR/direct-caller" >"$TEMP_DIR/direct-caller.out" ||
   ! printf 'Direct C source works\n' \
        | cmp -s - "$TEMP_DIR/direct-caller.out"
then
    echo "FAIL [foreign C] direct C source compilation failed"
    cat "$TEMP_DIR/direct-caller.out"
    exit 1
fi

if "$BIN" --asm \
    "$TEMP_DIR/direct-caller.lowc" \
    "$TEMP_DIR/direct-helper.c" \
    >"$TEMP_DIR/direct-asm.out" 2>&1
then
    echo "FAIL [foreign C] C source was accepted with --asm"
    exit 1
fi

test_copy_fixture "unsupported-foreign.lowc" || exit 1

if "$BIN" --asm "$TEMP_DIR/unsupported-foreign.lowc" \
    >"$TEMP_DIR/unsupported-foreign.out" 2>&1
then
    echo "FAIL [foreign C] unsupported foreign ABI was accepted"
    exit 1
fi

test_copy_fixture "uninitialized-local.lowc" || exit 1

test_copy_fixture "uninitialized-global.lowc" || exit 1

for scope in local global; do
    test_expect_diagnostic \
        "reject uninitialized $scope variables" \
        "variables must have an initializer" \
        "$TEMP_DIR/uninitialized-$scope.out" \
        "$BIN" --asm "$TEMP_DIR/uninitialized-$scope.lowc" ||
        exit 1
done

test_copy_fixture "format-input.lowc" || exit 1
cp "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-original.lowc"

if ! "$BIN" --format "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-stdout"
then
    echo "FAIL [format] fixed-style formatter did not run"
    exit 1
fi

test_copy_fixture "format-expected.lowc" || exit 1

if [ -s "$TEMP_DIR/format-stdout" ] ||
   ! cmp -s "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-expected.lowc" ||
   cmp -s "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-original.lowc"
then
    echo "FAIL [format] source was not overwritten with the fixed style"
    cat "$TEMP_DIR/format-input.lowc"
    exit 1
fi

cp "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-before-roundtrip.lowc"
if ! "$BIN" --format "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-roundtrip-stdout" ||
   [ -s "$TEMP_DIR/format-roundtrip-stdout" ] ||
   ! cmp -s "$TEMP_DIR/format-input.lowc" \
        "$TEMP_DIR/format-before-roundtrip.lowc"
then
    echo "FAIL [format] formatting was not idempotent"
    exit 1
fi

test_expect_diagnostic \
    "reject format combined with assembly output" \
    "--format cannot be combined with other output modes" \
    "$TEMP_DIR/format-conflict.out" \
    "$BIN" --format --asm "$TEMP_DIR/format-input.lowc" ||
    exit 1

for mode in --ast --tokens; do
    test_expect_diagnostic \
        "reject -debug combined with $mode" \
        "-debug cannot be combined with --ast, --tokens, or --format" \
        "$TEMP_DIR/debug-conflict-${mode#--}.out" \
        "$BIN" -debug "$mode" "$EXAMPLE_DIR/23_runtime.lowc" ||
        exit 1
done

test_expect_diagnostic \
    "reject -debug combined with --format" \
    "--format cannot be combined with other output modes, optimization/debug flags" \
    "$TEMP_DIR/debug-format-conflict.out" \
    "$BIN" -debug --format "$EXAMPLE_DIR/23_runtime.lowc" ||
    exit 1

test_expect_diagnostic \
    "reject -optimize combined with --format" \
    "--format cannot be combined with other output modes, optimization/debug flags" \
    "$TEMP_DIR/optimize-format-conflict.out" \
    "$BIN" -optimize --format "$EXAMPLE_DIR/23_runtime.lowc" ||
    exit 1

test_pass "$TEST_GROUP" "parser, types, foreign C, initializers, and formatting"
