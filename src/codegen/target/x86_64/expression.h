#ifndef LOWC_CODEGEN_TARGET_X86_64_EXPRESSION_H
#define LOWC_CODEGEN_TARGET_X86_64_EXPRESSION_H

#include "../../../parser/public.h"
#include "context.h"


/*
 * Generate the value of an expression.
 *
 * The resulting value is placed in rax.
 */
void codegen_x86_64_expression(
    codegen_x86_64_context *context,
    ast *expression
);


/*
 * Generate the address of an lvalue.
 *
 * The resulting address is placed in rax.
 *
 * This is used for:
 *
 *     &x
 *     *p
 *     values[index]
 *     object.member
 *     pointer->member
 */
void codegen_x86_64_lvalue_address(
    codegen_x86_64_context *context,
    ast *expression
);

int
codegen_x86_64_expression_type(
    codegen_x86_64_context *context,
    ast *expression,
    const char **type,
    unsigned long *pointer_depth
);

#endif
