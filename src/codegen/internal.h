#ifndef LOWC_CODEGEN_INTERNAL_H
#define LOWC_CODEGEN_INTERNAL_H

#include "public.h"
#include "output.h"
#include "target.h"

typedef struct codegen_context
{
    codegen_result *result;

    codegen_output output;

    const codegen_target *target;

    unsigned long label_counter;

} codegen_context;

int codegen_generate_program(
    codegen_context *context,
    ast *tree
);

int codegen_generate_node(
    codegen_context *context,
    ast *node
);

char *codegen_make_label(
    codegen_context *context,
    const char *prefix
);

void codegen_fail(
    codegen_context *context,
    const char *message
);

#endif
