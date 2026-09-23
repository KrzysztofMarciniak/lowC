#include "public.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
output_strdup(
    const char *value
)
{
    char *copy;

    if (value == NULL)
        return NULL;

    copy = malloc(strlen(value) + 1);

    if (copy == NULL)
        return NULL;

    strcpy(copy, value);

    return copy;
}

output_result
output(
    codegen_result codegen,
    flag_handler_options *options
)
{
    output_result result = {
        0
    };

    if (codegen.error)
    {
        result.error = 1;
        result.error_text =
            output_strdup("cannot output invalid codegen result");

        return result;
    }

    if (options == NULL)
    {
        result.error = 1;
        result.error_text =
            output_strdup("missing compiler options");

        return result;
    }

    if (!options->asm_output)
    {
        return result;
    }

    if (codegen.assembly == NULL)
    {
        result.error = 1;
        result.error_text =
            output_strdup("no assembly was generated");

        return result;
    }

    if (fputs(codegen.assembly, stdout) == EOF)
    {
        result.error = 1;
        result.error_text =
            output_strdup("failed to write assembly");

        return result;
    }

    return result;
}

void
output_handle_error(
    output_result result
)
{
    if (!result.error)
        return;

    fprintf(
        stderr,
        "output error: %s\n",
        result.error_text
            ? result.error_text
            : "unknown output error"
    );
}

void
output_free(
    output_result result
)
{
    free(result.error_text);
}
