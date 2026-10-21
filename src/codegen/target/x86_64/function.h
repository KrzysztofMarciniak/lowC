#ifndef LOWC_CODEGEN_TARGET_X86_64_FUNCTION_H
#define LOWC_CODEGEN_TARGET_X86_64_FUNCTION_H

#include "../../../parser/public.h"
#include "context.h"
#include "../../../ir/public.h"


/*
 * Generate one complete x86-64 function.
 *
 * This includes:
 *
 *     - local variable allocation
 *     - stack frame creation
 *     - parameter storage
 *     - function body
 *     - function epilogue
 */
void
codegen_x86_64_function(
    codegen_x86_64_context *context,
    const ir_function *function
);

#endif
