#ifndef LOWC_CODEGEN_TARGET_X86_64_STATEMENT_H
#define LOWC_CODEGEN_TARGET_X86_64_STATEMENT_H

#include "../../../parser/public.h"
#include "context.h"


/*
 * Generate x86-64 assembly for one statement.
 */
void
codegen_x86_64_statement(
    codegen_x86_64_context *context,
    ast *statement
);

void
codegen_x86_64_ir_statement(
    codegen_x86_64_context *context,
    const ir_function *function,
    const ir_instruction *instruction
);

#endif
