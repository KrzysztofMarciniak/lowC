#ifndef LOWC_CODEGEN_PUBLIC_H
#define LOWC_CODEGEN_PUBLIC_H

#include "../flag_handler/public.h"
#include "../parser/public.h"

typedef struct codegen_result
{
    int error;

    char *error_text;

    char *file;

    unsigned long line;
    unsigned long column;

    char *source_line;
    char *correct;

    /*
     * Complete generated assembly.
     */
    char *assembly;

} codegen_result;

codegen_result codegen(
    ast *tree,
    flag_handler_options *options
);

void codegen_handle_error(
    codegen_result result
);

void codegen_free(
    codegen_result result
);

#endif
