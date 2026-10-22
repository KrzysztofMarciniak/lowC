#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
TESTS_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$TESTS_DIR/lib/test_helpers.sh"
test_init "$1" "$2" "modules"

if ! "$BIN" "$EXAMPLE_DIR/23_runtime.lowc" \
    -optimize -o "$TEMP_DIR/program with space"
then
    echo "FAIL [executable] compiler invocation"
    exit 1
fi

if ! "$BIN" "$EXAMPLE_DIR/23_runtime.lowc" \
    -debug -o "$TEMP_DIR/program-unoptimized"
then
    echo "FAIL [executable] unoptimized compiler invocation"
    exit 1
fi

if ! "$BIN" "$EXAMPLE_DIR/23_runtime.lowc" \
    -release -o "$TEMP_DIR/program-release"
then
    echo "FAIL [executable] -release alias"
    exit 1
fi

if nm -a "$TEMP_DIR/program-release" 2>/dev/null | grep -q ' main$'; then
    echo "FAIL [executable] optimized release output was not stripped"
    exit 1
fi

if ! nm -a "$TEMP_DIR/program-unoptimized" 2>/dev/null | grep -q ' main$'; then
    echo "FAIL [executable] debug output did not retain symbols"
    exit 1
fi

if ! "$TEMP_DIR/program with space"; then
    echo "FAIL [executable] optimized program did not run successfully"
    exit 1
fi

if ! "$TEMP_DIR/program-release"; then
    echo "FAIL [executable] -release program did not run successfully"
    exit 1
fi

for removed_flag in -O0 -O1; do
    test_expect_diagnostic \
        "reject removed optimization flag $removed_flag" \
        "unknown flag" \
        "$TEMP_DIR/old-optimization-flag.out" \
        "$BIN" "$removed_flag" --asm "$EXAMPLE_DIR/23_runtime.lowc" ||
        exit 1
done

"$BIN" --asm -debug "$EXAMPLE_DIR/23_runtime.lowc" \
    >"$TEMP_DIR/debug-info.s" || exit 1
if ! grep -q '^\.file ' "$TEMP_DIR/debug-info.s" ||
   ! grep -q '\.loc ' "$TEMP_DIR/debug-info.s"
then
    echo "FAIL [debug] assembly lacked source mapping directives"
    exit 1
fi

if ! "$TEMP_DIR/program-unoptimized"; then
    echo "FAIL [executable] unoptimized program did not run successfully"
    exit 1
fi

if ! "$BIN" "$EXAMPLE_DIR/21_modules.lowc" -o "$TEMP_DIR/module-program" ||
   ! "$TEMP_DIR/module-program"
then
    echo "FAIL [modules] imported executable did not build and run"
    exit 1
fi

if ! "$BIN" --asm "$EXAMPLE_DIR/24_modules.lowc" >"$TEMP_DIR/modules.s"; then
    echo "FAIL [modules] could not generate assembly for imported declarations"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/modules.s" -x c \
    "$FIXTURE_DIR/harnesses/module_harness.c" \
    -o "$TEMP_DIR/modules-harness"
then
    echo "FAIL [modules] imported declarations did not assemble and link"
    exit 1
fi

if ! "$TEMP_DIR/modules-harness"; then
    echo "FAIL [modules] imported functions, structs, or globals returned incorrect results"
    exit 1
fi

if ! "$BIN" "$EXAMPLE_DIR/24_modules.lowc" -o "$TEMP_DIR/modules-program" ||
   ! "$TEMP_DIR/modules-program"
then
    echo "FAIL [modules] imported declarations did not build and run as an executable"
    exit 1
fi

MODULE_ACCESS_DIR="$FIXTURE_DIR/module_access"

if ! "$BIN" --asm "$MODULE_ACCESS_DIR/visibility_main.lowc" \
    >"$TEMP_DIR/visibility.s"
then
    echo "FAIL [modules] selected declarations were not visible"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/visibility.s" -x c \
    "$MODULE_ACCESS_DIR/harness.c" -o "$TEMP_DIR/visibility-harness" ||
   ! "$TEMP_DIR/visibility-harness"
then
    echo "FAIL [modules] selected declarations or private helpers returned incorrect results"
    exit 1
fi

if ! "$BIN" "$MODULE_ACCESS_DIR/visibility_main.lowc" \
    -o "$TEMP_DIR/visibility-program" ||
   ! "$TEMP_DIR/visibility-program"
then
    echo "FAIL [modules] visibility example did not build and run"
    exit 1
fi

mkdir -p "$TEMP_DIR/ir-pruning"
test_copy_fixture "ir-pruning/dependencies.lowc" || exit 1

test_copy_fixture "ir-pruning/main.lowc" || exit 1

if ! "$BIN" --asm "$TEMP_DIR/ir-pruning/main.lowc" \
    >"$TEMP_DIR/ir-pruning.s"
then
    echo "FAIL [IR] could not lower module dependency graph"
    exit 1
fi

for retained in entrypoint root_unused live_api live_helper; do
    if ! grep -q "^${retained}:" "$TEMP_DIR/ir-pruning.s"; then
        echo "FAIL [IR] required root or transitive declaration '$retained' was pruned"
        exit 1
    fi
done

for removed in dead_api dead_helper; do
    if grep -q "^${removed}:" "$TEMP_DIR/ir-pruning.s"; then
        echo "FAIL [IR] unused imported declaration '$removed' was emitted"
        exit 1
    fi
done

for hidden_case in function global struct dependency; do
    test_expect_diagnostic \
        "reject unimported $hidden_case" \
        "is not imported" \
        "$TEMP_DIR/hidden-${hidden_case}.out" \
        "$BIN" --asm "$MODULE_ACCESS_DIR/hidden_${hidden_case}.lowc" ||
        exit 1
done

test_pass "$TEST_GROUP" "executable modes, modules, visibility, and IR pruning"
