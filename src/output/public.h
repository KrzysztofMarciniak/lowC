#ifndef LOWC_OUTPUT_PUBLIC_H
#define LOWC_OUTPUT_PUBLIC_H

#include "../codegen/public.h"
#include "../flag_handler/public.h"

typedef struct output_result
{
    int error;
    char *error_text;
} output_result;

output_result
output(
    codegen_result codegen,
    flag_handler_options *options,
    files *input_files
);

void
output_handle_error(
    output_result result
);

void
output_free(
    output_result result
);

#endif
