#ifndef LOWC_CODEGEN_OUTPUT_H
#define LOWC_CODEGEN_OUTPUT_H

#include <stddef.h>

typedef struct codegen_output
{
    char *data;
    size_t size;
    size_t capacity;
} codegen_output;

void codegen_output_init(
    codegen_output *output
);

void codegen_output_free(
    codegen_output *output
);

int codegen_output_write(
    codegen_output *output,
    const char *text
);

int codegen_output_printf(
    codegen_output *output,
    const char *format,
    ...
);

#endif
