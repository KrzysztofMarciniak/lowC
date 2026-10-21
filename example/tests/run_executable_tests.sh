#!/bin/bash

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
BIN="${1:-$SCRIPT_DIR/../../build/lowc}"
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/lowc-tests.XXXXXX") || exit 1

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

cat >"$TEMP_DIR/empty-parameters.lowc" <<'EOF'
void missing_marker() {
    return;
}
EOF

if "$BIN" --ast "$TEMP_DIR/empty-parameters.lowc" \
    >"$TEMP_DIR/empty-parameters.out" 2>&1
then
    echo "FAIL [parser] accepted a function definition with empty parameters"
    exit 1
fi

if ! grep -q "functions with no parameters must use '(void)'" \
    "$TEMP_DIR/empty-parameters.out"
then
    echo "FAIL [parser] missing '(void)' diagnostic"
    cat "$TEMP_DIR/empty-parameters.out"
    exit 1
fi

cat >"$TEMP_DIR/void-parameters.lowc" <<'EOF'
void no_arguments(void) {
    return;
}

void takes_void_pointer(void *pointer) {
    return;
}

void entrypoint(void) {
    no_arguments();
    return;
}
EOF

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

if ! "$BIN" --asm "$SCRIPT_DIR/19_types.lowc" \
    >"$TEMP_DIR/type-widths.s" ||
   ! grep -q "BYTE PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "WORD PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "DWORD PTR" "$TEMP_DIR/type-widths.s" ||
   ! grep -q "QWORD PTR" "$TEMP_DIR/type-widths.s"
then
    echo "FAIL [types] byte widths did not generate the expected x86-64 stores"
    exit 1
fi

cat >"$TEMP_DIR/type-widths-harness.c" <<'EOF'
unsigned long test_widths(void);

int
main(void)
{
    return test_widths() == 15 ? 0 : 1;
}
EOF

if ! cc -x assembler "$TEMP_DIR/type-widths.s" -x c \
    "$TEMP_DIR/type-widths-harness.c" -o "$TEMP_DIR/type-widths" ||
   ! "$TEMP_DIR/type-widths"
then
    echo "FAIL [types] byte-width arrays did not execute correctly"
    exit 1
fi

cat >"$TEMP_DIR/foreign-fixture.c" <<'EOF'
#include <stdint.h>

uint64_t lowc_foreign_add(uint64_t left, uint64_t right)
{
    return left + right;
}

uint64_t lowc_foreign_length(const unsigned char *text)
{
    uint64_t length = 0;

    while (text[length] != 0)
        length++;

    return length;
}

const unsigned char *lowc_foreign_identity(const unsigned char *text)
{
    return text;
}

uint8_t lowc_foreign_unsigned_byte(void)
{
    return 200;
}

int8_t lowc_foreign_signed_byte(void)
{
    return -7;
}
EOF

if ! cc -c "$TEMP_DIR/foreign-fixture.c" \
    -o "$TEMP_DIR/foreign-fixture.o" ||
   ! ar rcs "$TEMP_DIR/liblowc_foreign_fixture.a" \
        "$TEMP_DIR/foreign-fixture.o"
then
    echo "FAIL [foreign C] could not build fixture library"
    exit 1
fi

cat >"$TEMP_DIR/foreign-caller.lowc" <<'EOF'
foreign c u8 lowc_foreign_add(u8 left, u8 right);
foreign c u8 lowc_foreign_length(u1 *text);
foreign c u1 *lowc_foreign_identity(u1 *text);
foreign c u1 lowc_foreign_unsigned_byte(void);
foreign c s1 lowc_foreign_signed_byte(void);
foreign c s4 puts(u1 *text);

void entrypoint(void) {
    if (
        lowc_foreign_add(20, 22) == 42 &&
        lowc_foreign_length(
            lowc_foreign_identity("C ABI")
        ) == 5 &&
        lowc_foreign_unsigned_byte() == 200 &&
        lowc_foreign_signed_byte() == -7
    ) {
        puts("C interoperability works");
    }
    return;
}
EOF

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

cat >"$TEMP_DIR/direct-helper.c" <<'EOF'
#include <stdint.h>

uint64_t lowc_direct_add(uint64_t left, uint64_t right)
{
    return left + right;
}
EOF

cat >"$TEMP_DIR/direct-caller.lowc" <<'EOF'
foreign c u8 lowc_direct_add(u8 left, u8 right);
foreign c s4 puts(u1 *text);

void entrypoint(void) {
    if (lowc_direct_add(20, 22) == 42) {
        puts("Direct C source works");
    }
    return;
}
EOF

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

cat >"$TEMP_DIR/unsupported-foreign.lowc" <<'EOF'
foreign forth u8 unsupported(void);
void entrypoint(void) {
    return;
}
EOF

if "$BIN" --asm "$TEMP_DIR/unsupported-foreign.lowc" \
    >"$TEMP_DIR/unsupported-foreign.out" 2>&1
then
    echo "FAIL [foreign C] unsupported foreign ABI was accepted"
    exit 1
fi

cat >"$TEMP_DIR/uninitialized-local.lowc" <<'EOF'
void entrypoint(void) {
    u1 value;
    return;
}
EOF

cat >"$TEMP_DIR/uninitialized-global.lowc" <<'EOF'
u1 value;
void entrypoint(void) {
    return;
}
EOF

for scope in local global; do
    if "$BIN" --asm "$TEMP_DIR/uninitialized-$scope.lowc" \
        >"$TEMP_DIR/uninitialized-$scope.out" 2>&1 ||
       ! grep -q "variables must have an initializer" \
            "$TEMP_DIR/uninitialized-$scope.out"
    then
        echo "FAIL [initializers] uninitialized $scope variable was accepted"
        cat "$TEMP_DIR/uninitialized-$scope.out"
        exit 1
    fi
done

cat >"$TEMP_DIR/format-input.lowc" <<'EOF'
#include "z.lowc" : "z";
#include "a.lowc" : "a";
void entrypoint(void){u8 b=2;u8 a=1;if(a){b=b+1;long_name=2;}}
EOF
cp "$TEMP_DIR/format-input.lowc" "$TEMP_DIR/format-original.lowc"

if ! "$BIN" --format "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-stdout"
then
    echo "FAIL [format] fixed-style formatter did not run"
    exit 1
fi

cat >"$TEMP_DIR/format-expected.lowc" <<'EOF'
#include "a.lowc" : "a";
#include "z.lowc" : "z";

void entrypoint(void) {
        u8 b = 2;
        u8 a = 1;
        if (a) {
                b         = b + 1;
                long_name = 2;
        }
}
EOF

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

if "$BIN" --format --asm "$TEMP_DIR/format-input.lowc" \
    >"$TEMP_DIR/format-conflict.out" 2>&1
then
    echo "FAIL [format] conflicting output modes were accepted"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/23_runtime.lowc" \
    -optimize -o "$TEMP_DIR/program with space"
then
    echo "FAIL [executable] compiler invocation"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/23_runtime.lowc" \
    -debug -o "$TEMP_DIR/program-unoptimized"
then
    echo "FAIL [executable] unoptimized compiler invocation"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/23_runtime.lowc" \
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
    if "$BIN" "$removed_flag" --asm "$SCRIPT_DIR/23_runtime.lowc" \
        >"$TEMP_DIR/old-optimization-flag.out" 2>&1 ||
       ! grep -q "unknown flag" "$TEMP_DIR/old-optimization-flag.out"
    then
        echo "FAIL [flags] accepted removed $removed_flag option"
        exit 1
    fi
done

"$BIN" --asm -debug "$SCRIPT_DIR/23_runtime.lowc" \
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

if ! "$BIN" "$SCRIPT_DIR/21_modules.lowc" -o "$TEMP_DIR/module-program" ||
   ! "$TEMP_DIR/module-program"
then
    echo "FAIL [modules] imported executable did not build and run"
    exit 1
fi

if ! "$BIN" --asm "$SCRIPT_DIR/24_modules.lowc" >"$TEMP_DIR/modules.s"; then
    echo "FAIL [modules] could not generate assembly for imported declarations"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/modules.s" -x c \
    "$SCRIPT_DIR/module_harness.c" -o "$TEMP_DIR/modules-harness"
then
    echo "FAIL [modules] imported declarations did not assemble and link"
    exit 1
fi

if ! "$TEMP_DIR/modules-harness"; then
    echo "FAIL [modules] imported functions, structs, or globals returned incorrect results"
    exit 1
fi

if ! "$BIN" "$SCRIPT_DIR/24_modules.lowc" -o "$TEMP_DIR/modules-program" ||
   ! "$TEMP_DIR/modules-program"
then
    echo "FAIL [modules] imported declarations did not build and run as an executable"
    exit 1
fi

MODULE_ACCESS_DIR="$SCRIPT_DIR/module_access"

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
cat >"$TEMP_DIR/ir-pruning/dependencies.lowc" <<'EOF'
s8 live_helper(s8 value) {
    return value + 1;
}

s8 live_api(s8 value) {
    return live_helper(value);
}

s8 dead_helper(s8 value) {
    return value + 100;
}

s8 dead_api(s8 value) {
    return dead_helper(value);
}
EOF

cat >"$TEMP_DIR/ir-pruning/main.lowc" <<'EOF'
#include "dependencies.lowc" : "live_api", "dead_api";

s8 root_unused(void) {
    return 99;
}

void entrypoint(void) {
    s8 result = live_api(4);
    return;
}
EOF

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
    if "$BIN" --asm "$MODULE_ACCESS_DIR/hidden_${hidden_case}.lowc" \
        >"$TEMP_DIR/hidden-${hidden_case}.out" 2>&1
    then
        echo "FAIL [modules] unimported ${hidden_case} was accessible"
        exit 1
    fi

    if ! grep -q "is not imported" \
        "$TEMP_DIR/hidden-${hidden_case}.out"
    then
        echo "FAIL [modules] ${hidden_case} visibility failed for an unrelated reason"
        cat "$TEMP_DIR/hidden-${hidden_case}.out"
        exit 1
    fi
done

"$BIN" --asm -debug "$SCRIPT_DIR/23_runtime.lowc" >"$TEMP_DIR/unoptimized.s" || exit 1
"$BIN" --asm -optimize "$SCRIPT_DIR/23_runtime.lowc" >"$TEMP_DIR/optimized.s" || exit 1
"$BIN" --asm "$SCRIPT_DIR/23_runtime.lowc" >"$TEMP_DIR/default.s" || exit 1
"$BIN" --asm -debug -optimize "$SCRIPT_DIR/23_runtime.lowc" \
    >"$TEMP_DIR/debug-optimized.s" || exit 1

unoptimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/unoptimized.s" || true)
optimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/optimized.s" || true)
default_pushes=$(grep -c 'push rax' "$TEMP_DIR/default.s" || true)
debug_optimized_pushes=$(grep -c 'push rax' "$TEMP_DIR/debug-optimized.s" || true)

if [ "$optimized_pushes" -ge "$unoptimized_pushes" ]; then
    echo "FAIL [optimization] optimized assembly did not reduce stack traffic"
    exit 1
fi
if [ "$default_pushes" -ne "$optimized_pushes" ]; then
    echo "FAIL [optimization] optimization was not enabled by default"
    exit 1
fi
if [ "$debug_optimized_pushes" -ne "$optimized_pushes" ]; then
    echo "FAIL [optimization] -debug -optimize did not keep optimization enabled"
    exit 1
fi

cat >"$TEMP_DIR/constant-branch.lowc" <<'EOF'
void entrypoint(void) {
    if (1) {
        return;
    } else {
        return;
    }
}
EOF

if ! "$BIN" --asm -debug "$TEMP_DIR/constant-branch.lowc" \
    >"$TEMP_DIR/constant-branch-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$TEMP_DIR/constant-branch.lowc" \
    >"$TEMP_DIR/constant-branch-optimized.s" ||
   ! grep -q 'jne .L' "$TEMP_DIR/constant-branch-unoptimized.s" ||
   grep -q 'jne .L' "$TEMP_DIR/constant-branch-optimized.s"
then
    echo "FAIL [optimization] constant branch was not simplified"
    exit 1
fi

if ! "$BIN" --asm -debug "$SCRIPT_DIR/test_dead_code.lowc" \
    >"$TEMP_DIR/dead-code-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$SCRIPT_DIR/test_dead_code.lowc" \
    >"$TEMP_DIR/dead-code-optimized.s" ||
   ! grep -q 'call dead_effect' "$TEMP_DIR/dead-code-unoptimized.s" ||
   grep -q 'call dead_effect' "$TEMP_DIR/dead-code-optimized.s" ||
   ! grep -q 'call live_effect' "$TEMP_DIR/dead-code-optimized.s"
then
    echo "FAIL [optimization] unreachable effectful block was not removed safely"
    exit 1
fi

for mode in unoptimized optimized; do
    if ! cc -x assembler "$TEMP_DIR/dead-code-$mode.s" -x c \
        "$SCRIPT_DIR/test_dead_code_harness.c" \
        -o "$TEMP_DIR/dead-code-$mode" ||
       ! "$TEMP_DIR/dead-code-$mode"
    then
        echo "FAIL [optimization] $mode dead-code fixture changed runtime behavior"
        exit 1
    fi
done

cat >"$TEMP_DIR/pure-expression.lowc" <<'EOF'
u1 expression_input = 42;

void entrypoint(void) {
    expression_input + 7;
    return;
}
EOF

if ! "$BIN" --asm -debug "$TEMP_DIR/pure-expression.lowc" \
    >"$TEMP_DIR/pure-expression-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$TEMP_DIR/pure-expression.lowc" \
    >"$TEMP_DIR/pure-expression-optimized.s" ||
   ! grep -q 'push rax' "$TEMP_DIR/pure-expression-unoptimized.s" ||
   grep -q 'push rax' "$TEMP_DIR/pure-expression-optimized.s"
then
    echo "FAIL [optimization] unused pure expression was not removed"
    exit 1
fi

cat >"$TEMP_DIR/trapping-expression.lowc" <<'EOF'
u8 divisor = 0;

void entrypoint(void) {
    divisor / divisor;
    return;
}
EOF

if ! "$BIN" --asm -optimize "$TEMP_DIR/trapping-expression.lowc" \
    >"$TEMP_DIR/trapping-expression-optimized.s" ||
   ! grep -q 'idiv rcx' "$TEMP_DIR/trapping-expression-optimized.s"
then
    echo "FAIL [optimization] potentially trapping expression was removed"
    exit 1
fi

cat >"$TEMP_DIR/constant-call.lowc" <<'EOF'
u1 result_x = 0;
u1 result_y = 0;

u1, u1 minushundred(u1 first, u1 second) {
    return first - 100, second - 100;
}

void consume(u1 value) {
    return;
}

void text(u1 *message) {
    return;
}

void entrypoint(void) {
    u1 x = 100;
    u1 y = 200;
    text("before");
    consume(x);
    consume(y);
    (x, y) = minushundred(x, y);
    text("after");
    consume(x);
    consume(y);
    result_x = x;
    result_y = y;
    return;
}
EOF

cat >"$TEMP_DIR/constant-call-harness.c" <<'EOF'
extern unsigned char result_x;
extern unsigned char result_y;
void entrypoint(void);

int
main(void)
{
    entrypoint();
    return result_x == 0 && result_y == 100 ? 0 : 1;
}
EOF

if ! "$BIN" --asm -debug "$TEMP_DIR/constant-call.lowc" \
    >"$TEMP_DIR/constant-call-unoptimized.s" ||
   ! "$BIN" --asm -optimize "$TEMP_DIR/constant-call.lowc" \
    >"$TEMP_DIR/constant-call-optimized.s" ||
   ! grep -q 'call minushundred' "$TEMP_DIR/constant-call-unoptimized.s" ||
   grep -q 'call minushundred' "$TEMP_DIR/constant-call-optimized.s"
then
    echo "FAIL [optimization] pure constant multi-return call was not precomputed"
    exit 1
fi

for mode in unoptimized optimized; do
    if ! cc -x assembler "$TEMP_DIR/constant-call-$mode.s" -x c \
        "$TEMP_DIR/constant-call-harness.c" \
        -o "$TEMP_DIR/constant-call-$mode" ||
       ! "$TEMP_DIR/constant-call-$mode"
    then
        echo "FAIL [optimization] $mode constant-call result changed program behavior"
        exit 1
    fi
done

if ! "$BIN" -debug -optimize "$TEMP_DIR/constant-call.lowc" \
    -o "$TEMP_DIR/constant-call-release-debug" ||
   nm -a "$TEMP_DIR/constant-call-release-debug" 2>/dev/null \
       | grep -q ' minushundred$'
then
    echo "FAIL [optimization] release linker retained the precomputed function"
    exit 1
fi

if ! "$BIN" -debug "$TEMP_DIR/constant-call.lowc" \
    -o "$TEMP_DIR/constant-call-debug"
then
    echo "FAIL [optimization] could not build the unoptimized size fixture"
    exit 1
fi

unoptimized_text_size=$(
    size "$TEMP_DIR/constant-call-debug" | awk 'NR == 2 { print $1 }'
)
optimized_text_size=$(
    size "$TEMP_DIR/constant-call-release-debug" |
        awk 'NR == 2 { print $1 }'
)
if [ -z "$unoptimized_text_size" ] ||
   [ -z "$optimized_text_size" ] ||
   [ "$optimized_text_size" -ge "$unoptimized_text_size" ]
then
    echo "FAIL [optimization] precomputed release did not reduce executable text size"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/optimized.s" -x c \
    "$SCRIPT_DIR/runtime_harness.c" -o "$TEMP_DIR/harness"
then
    echo "FAIL [runtime] could not link generated assembly with ABI harness"
    exit 1
fi

if ! cc -x assembler "$TEMP_DIR/unoptimized.s" -x c \
    "$SCRIPT_DIR/runtime_harness.c" -o "$TEMP_DIR/harness-unoptimized"
then
    echo "FAIL [runtime] could not link unoptimized assembly with ABI harness"
    exit 1
fi

if ! "$TEMP_DIR/harness"; then
    echo "FAIL [runtime] generated functions returned incorrect results"
    exit 1
fi

if ! "$TEMP_DIR/harness-unoptimized"; then
    echo "FAIL [runtime] unoptimized functions returned incorrect results"
    exit 1
fi

for mode in unoptimized optimized; do
    if [ "$mode" = optimized ]; then
        optimization=-optimize
    else
        optimization=-debug
    fi

    if ! "$BIN" --asm $optimization \
        "$SCRIPT_DIR/27_multiple_returns.lowc" \
        >"$TEMP_DIR/multiple-returns-$mode.s" ||
       ! cc -x assembler "$TEMP_DIR/multiple-returns-$mode.s" -x c \
           "$SCRIPT_DIR/multiple_returns_harness.c" \
           -o "$TEMP_DIR/multiple-returns-$mode" ||
       ! "$TEMP_DIR/multiple-returns-$mode"
    then
        echo "FAIL [multiple returns] $mode values were not returned and destructured correctly"
        exit 1
    fi
done

cat >"$TEMP_DIR/multiple-return-count.lowc" <<'EOF'
u1, u1 pair(void) {
    return 1;
}
EOF

if "$BIN" --asm "$TEMP_DIR/multiple-return-count.lowc" \
    >"$TEMP_DIR/multiple-return-count.out" 2>&1 ||
   ! grep -q "wrong number of values" \
       "$TEMP_DIR/multiple-return-count.out"
then
    echo "FAIL [multiple returns] accepted a return statement with the wrong number of values"
    exit 1
fi

cat >"$TEMP_DIR/multiple-return-targets.lowc" <<'EOF'
u1, u1 pair(void) {
    return 1, 2;
}

void entrypoint(void) {
    u1 first = 0;
    u1 second = 0;
    u1 third = 0;
    (first, second, third) = pair();
}
EOF

if "$BIN" --asm "$TEMP_DIR/multiple-return-targets.lowc" \
    >"$TEMP_DIR/multiple-return-targets.out" 2>&1 ||
   ! grep -q "wrong number of targets" \
       "$TEMP_DIR/multiple-return-targets.out"
then
    echo "FAIL [multiple returns] accepted tuple assignment with the wrong number of targets"
    exit 1
fi

cat >"$TEMP_DIR/multiple-return-type.lowc" <<'EOF'
u1, u1 pair(void) {
    return 1, 2;
}

void entrypoint(void) {
    u1 first = 0;
    u1 *second = 0;
    (first, second) = pair();
}
EOF

if "$BIN" --asm "$TEMP_DIR/multiple-return-type.lowc" \
    >"$TEMP_DIR/multiple-return-type.out" 2>&1 ||
   ! grep -q "incompatible type" "$TEMP_DIR/multiple-return-type.out"
then
    echo "FAIL [multiple returns] accepted a tuple target with an incompatible type"
    exit 1
fi

cat >"$TEMP_DIR/foreign-multiple-return.lowc" <<'EOF'
foreign c u1, u1 invalid_foreign(void);
EOF

if "$BIN" --ast "$TEMP_DIR/foreign-multiple-return.lowc" \
    >"$TEMP_DIR/foreign-multiple-return.out" 2>&1 ||
   ! grep -q "foreign functions must declare exactly one return type" \
       "$TEMP_DIR/foreign-multiple-return.out"
then
    echo "FAIL [foreign C] accepted multiple return types"
    exit 1
fi

cat >"$TEMP_DIR/too-many-lowc-parameters.lowc" <<'EOF'
u8 unsupported(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, u8 g) {
    return a;
}
EOF

if "$BIN" --asm "$TEMP_DIR/too-many-lowc-parameters.lowc" \
    >"$TEMP_DIR/too-many-lowc-parameters.out" 2>&1 ||
   ! grep -q "lowC functions support at most six parameters" \
       "$TEMP_DIR/too-many-lowc-parameters.out"
then
    echo "FAIL [functions] accepted more than six lowC function parameters"
    exit 1
fi

if ! make -C "$SCRIPT_DIR/../.." install \
    DESTDIR="$TEMP_DIR" PREFIX=/usr/local >/dev/null
then
    echo "FAIL [standard library] could not stage installed public modules"
    exit 1
fi

cat >"$TEMP_DIR/standard-library-print.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/print.lowc" : "print";
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" : "strlen";

void entrypoint(void) {
    u8 length = strlen("Hello, World\\n");
    print("Hello, World\\n");
}
EOF

cat >"$TEMP_DIR/entrypoint_harness.c" <<'EOF'
void entrypoint(void);

int
main(void)
{
    entrypoint();
    return 0;
}
EOF

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

cat >"$TEMP_DIR/standard-library-numbers.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/print.lowc" :
    "print", "print_u1", "print_u2", "print_u4", "print_u8",
    "print_s1", "print_s2", "print_s4", "print_s8";
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" : "parse_u8";

u8 standard_library_numbers_test(void) {
    u8 value = 0;
    s8 minimum = 0;

    if (!parse_u8("0", &value) || value != 0) {
        return 1;
    }
    print_u8(value);
    print("|");

    print_u1(1);
    print("|");
    print_u2(2);
    print("|");
    print_u4(4);
    print("|");
    print_u8(8);
    print("|");

    if (!parse_u8("18446744073709551615", &value)) {
        return 2;
    }
    print_u8(value);
    print("|");

    if (!parse_u8("00000000000000000042", &value) || value != 42) {
        return 3;
    }

    if (parse_u8("", &value) || value != 42) {
        return 4;
    }
    if (parse_u8("-1", &value) || value != 42) {
        return 5;
    }
    if (parse_u8("12x", &value) || value != 42) {
        return 6;
    }
    if (parse_u8("18446744073709551616", &value) || value != 42) {
        return 7;
    }

    print_s1(-1);
    print("|");
    print_s2(-1);
    print("|");
    print_s4(-1);
    print("|");
    print_s8(-1);
    print("|");
    asm("mov %0, 0x8000000000000000" : "=rax"(minimum) : :);
    print_s8(minimum);
    print("|");
    print_s8(9223372036854775807);
    print("|done\n");

    return 0;
}
EOF

cat >"$TEMP_DIR/standard-library-numbers-harness.c" <<'EOF'
unsigned long standard_library_numbers_test(void);

int
main(void)
{
    return standard_library_numbers_test() == 0 ? 0 : 1;
}
EOF

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

cat >"$TEMP_DIR/standard-library-data.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/string.lowc" :
    "strlen", "strcmp", "strcpy", "strcat", "strdup";
#include "$TEMP_DIR/usr/local/share/lowc/public/memory.lowc" :
    "alloc", "dealloc";
#include "$TEMP_DIR/usr/local/share/lowc/public/hashmap.lowc" :
    "HashMap", "hashmap_init", "hashmap_set", "hashmap_get",
    "hashmap_remove", "hashmap_deinit";

u8 standard_library_data_test(void) {
    u1 *buffer = 0;
    u1 *copy = 0;
    struct HashMap map = 0;
    u8 value = 0;
    u8 high_key = 0;
    u8 index = 0;

    buffer = alloc(32);
    strcpy(buffer, "low");
    strcat(buffer, "C");
    if (strcmp(buffer, "lowC") != 0 || strlen(buffer) != 4) {
        return 1;
    }

    copy = strdup(buffer);
    if (strcmp(copy, "lowC") != 0 || dealloc(copy) == 0) {
        return 2;
    }

    hashmap_init(&map, 8);
    for (index = 0; index < 12; index = index + 1) {
        hashmap_set(&map, index, index * 7);
    }

    asm("mov %0, 0x8000000000000000" : "=rax"(high_key) : :);
    hashmap_set(&map, high_key, 99);
    if (hashmap_get(&map, high_key, &value) == 0 || value != 99) {
        return 3;
    }

    if (hashmap_get(&map, 8, &value) == 0 || value != 56) {
        return 4;
    }

    if (hashmap_remove(&map, 8) == 0 || hashmap_get(&map, 8, &value) != 0) {
        return 5;
    }

    hashmap_set(&map, 24, 168);
    if (hashmap_get(&map, 24, &value) == 0 || value != 168) {
        return 6;
    }

    if (hashmap_get(&map, 9, &value) == 0 || value != 63) {
        return 7;
    }

    if (hashmap_deinit(&map) == 0 || dealloc(buffer) == 0) {
        return 8;
    }

    return 0;
}
EOF

cat >"$TEMP_DIR/standard-library-data-harness.c" <<'EOF'
unsigned long standard_library_data_test(void);

int
main(void)
{
    return standard_library_data_test() == 0 ? 0 : 1;
}
EOF

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

if ! "$BIN" --asm "$SCRIPT_DIR/26_inline_asm.lowc" \
    >"$TEMP_DIR/inline-asm.s"
then
    echo "FAIL [inline assembly] valid assembly did not compile"
    exit 1
fi

cat >"$TEMP_DIR/inline-asm-harness.c" <<'EOF'
unsigned long inline_assembly_move(void);
unsigned long inline_assembly_syscall_pid(void);
unsigned long inline_assembly_callee_saved(void);
unsigned long inline_assembly_call(void);

int
main(void)
{
    return inline_assembly_move() == 42 &&
           inline_assembly_syscall_pid() > 0 &&
           inline_assembly_callee_saved() == 17 &&
           inline_assembly_call() == 23
        ? 0
        : 1;
}
EOF

if ! cc -x assembler "$TEMP_DIR/inline-asm.s" -x c \
    "$TEMP_DIR/inline-asm-harness.c" -o "$TEMP_DIR/inline-asm-program" ||
   ! "$TEMP_DIR/inline-asm-program"
then
    echo "FAIL [inline assembly] outputs, syscall, or callee-saved handling failed"
    exit 1
fi

cat >"$TEMP_DIR/standard-library-args.lowc" <<EOF
#include "$TEMP_DIR/usr/local/share/lowc/public/args.lowc" :
    "args_count", "args_get";
#include "$TEMP_DIR/usr/local/share/lowc/public/print.lowc" :
    "print", "print_u8";

void entrypoint(void) {
    u8 count = 0;
    u1 *argument = 0;

    count = args_count();
    print_u8(count);
    print("|");

    if (args_get(0, &argument)) {
        print(argument);
    } else {
        print("missing");
    }
    print("|");

    if (args_get(1, &argument)) {
        print(argument);
    } else {
        print("missing");
    }
    print("|");

    if (args_get(2, &argument)) {
        print(argument);
    } else {
        print("missing");
    }
    print("|");

    argument = "unchanged";
    if (args_get(count, &argument)) {
        print("unexpected");
    }
    print(argument);
    print("\\n");
}
EOF

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

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
u8 invalid_output(void) {
    u8 result = 0;
    asm("nop" : "=rsp"(result) : :);
    return result;
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "invalid inline assembly output register constraint" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] reserved rsp output was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
void invalid_output_lvalue(void) {
    asm("nop" : "=rax"(42) : :);
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "inline assembly output must be an assignable lvalue" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] non-lvalue output was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
void invalid_input_register(void) {
    asm("nop" : : "foo"(1) :);
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "invalid inline assembly input register constraint" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] unknown input register was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

cat >"$TEMP_DIR/invalid-inline-asm.lowc" <<'EOF'
void invalid_placeholder(void) {
    asm("mov %0, %1" : : :);
}
EOF

if "$BIN" --asm "$TEMP_DIR/invalid-inline-asm.lowc" \
    >"$TEMP_DIR/invalid-inline-asm.out" 2>&1 ||
   ! grep -q "inline assembly operand placeholder is out of range" \
        "$TEMP_DIR/invalid-inline-asm.out"
then
    echo "FAIL [inline assembly] invalid placeholder was not rejected"
    cat "$TEMP_DIR/invalid-inline-asm.out"
    exit 1
fi

echo "ok   [modules] imported functions, structs, and globals assemble, link, and run"
echo "ok   [modules] unimported functions, structs, and globals are rejected"
echo "ok   [IR] unused imported declarations are pruned with transitive dependencies preserved"
echo "ok   [optimization] constant folding, branch simplification, and dead-code removal work"
echo "ok   [executable] optimized and unoptimized output assembles, links, and runs"
echo "ok   [standard library] installed print module writes exact output"
echo "ok   [standard library] CLI arguments include argv[0] and are bounds checked"
echo "ok   [inline assembly] register operands, syscall, clobbers, and validation work"
