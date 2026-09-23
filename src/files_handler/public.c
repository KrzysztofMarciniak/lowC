#include "public.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

files_handler_result
files_handler(
    files *files
)
{
    source_file *values = 0;
    int count = 0;

    for (int index = 0;
         index < files->count;
         index++)
    {
        source_file *expanded = 0;
        int expanded_count = 0;

        int result =
            files_handler_expand(
                files->values[index],
                &expanded,
                &expanded_count
            );

        if (result == 1)
        {
            return (files_handler_result) {
                .error = 1,
                .error_text =
                    "failed to find or read source file."
            };
        }

        if (result == 2)
        {
            return (files_handler_result) {
                .error = 1,
                .error_text =
                    "input is not a .lowc source file."
            };
        }

        source_file *new_values =
            realloc(
                values,
                sizeof(source_file) *
                (size_t)(count + expanded_count)
            );

        if (!new_values)
        {
            files_handler_free(
                expanded,
                expanded_count
            );

            files_handler_free(
                values,
                count
            );

            return (files_handler_result) {
                .error = 1,
                .error_text =
                    "failed to allocate source files."
            };
        }

        values = new_values;

        memcpy(
            &values[count],
            expanded,
            sizeof(source_file) *
            (size_t)expanded_count
        );

        count += expanded_count;

        free(expanded);
    }

    return (files_handler_result) {
        .error = 0,
        .error_text = 0,
        .count = count,
        .values = values
    };
}

void
files_handler_handle_error(
    files_handler_result result
)
{
    fprintf(
        stderr,
        "files error: %s\n",
        result.error_text
    );

    fprintf(
        stderr,
        "use --help\n"
    );
}
