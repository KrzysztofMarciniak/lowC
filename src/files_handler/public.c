#include "public.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
files_handler_copy_text(
    const char *text,
    size_t length
)
{
    char *copy = malloc(length + 1);

    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static int
files_handler_next_import(
    const char **source,
    char **import_path
)
{
    const char *line = *source;

    while (*line != '\0') {
        const char *line_end = strchr(line, '\n');
        const char *cursor = line;
        const char *quote_end;

        if (!line_end) {
            line_end = line + strlen(line);
        }

        while (cursor < line_end &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')) {
            cursor++;
        }

        if (
            line_end - cursor >= 8 &&
            memcmp(cursor, "#include", 8) == 0
        ) {
            cursor += 8;
            while (cursor < line_end &&
                   (*cursor == ' ' || *cursor == '\t')) {
                cursor++;
            }

            if (cursor < line_end && *cursor == '"') {
                cursor++;
                quote_end = memchr(cursor, '"', (size_t)(line_end - cursor));
                if (quote_end) {
                    *import_path = files_handler_copy_text(
                        cursor,
                        (size_t)(quote_end - cursor)
                    );
                    if (!*import_path) {
                        return -1;
                    }

                    *source = *line_end == '\0'
                        ? line_end
                        : line_end + 1;
                    return 1;
                }
            }
        }

        line = *line_end == '\0' ? line_end : line_end + 1;
    }

    *source = line;
    return 0;
}

static char *
files_handler_resolve_import(
    const char *source_path,
    const char *import_path
)
{
    const char *slash;
    size_t directory_length;
    size_t import_length;
    char *resolved;

    if (import_path[0] == '/') {
        directory_length = 0;
    } else {
        slash = strrchr(source_path, '/');
        directory_length = slash
            ? (size_t)(slash - source_path + 1)
            : 0;
    }

    import_length = strlen(import_path);
    resolved = malloc(directory_length + import_length + 1);
    if (!resolved) {
        return NULL;
    }

    if (directory_length != 0) {
        memcpy(resolved, source_path, directory_length);
    }
    memcpy(
        resolved + directory_length,
        import_path,
        import_length + 1
    );

    return resolved;
}

static int
files_handler_has_path(
    source_file *files,
    int count,
    const char *path
)
{
    for (int index = 0; index < count; index++) {
        if (strcmp(files[index].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int
files_handler_append(
    source_file **files,
    int *count,
    source_file *added,
    int added_count
)
{
    for (int index = 0; index < added_count; index++) {
        source_file *new_values;

        if (files_handler_has_path(*files, *count, added[index].path)) {
            free(added[index].path);
            free(added[index].content);
            added[index].path = NULL;
            added[index].content = NULL;
            continue;
        }

        new_values = realloc(
            *files,
            sizeof(source_file) * (size_t)(*count + 1)
        );
        if (!new_values) {
            return 1;
        }

        *files = new_values;
        (*files)[*count] = added[index];
        added[index].path = NULL;
        added[index].content = NULL;
        (*count)++;
    }

    return 0;
}

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
            files_handler_free(values, count);
            return (files_handler_result) {
                .error = 1,
                .error_text =
                    "failed to find or read source file."
            };
        }

        if (result == 2)
        {
            files_handler_free(values, count);
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

            files_handler_free(values, count);

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

    for (int index = 0; index < count; index++) {
        const char *source = values[index].content;
        char *import_path;
        int import_result;

        while ((import_result = files_handler_next_import(
                    &source,
                    &import_path
                )) > 0) {
            char *resolved = files_handler_resolve_import(
                values[index].path,
                import_path
            );
            source_file *expanded = NULL;
            int expanded_count = 0;
            int result;

            free(import_path);

            if (!resolved) {
                files_handler_free(values, count);
                return (files_handler_result) {
                    .error = 1,
                    .error_text = "failed to resolve imported source file."
                };
            }

            if (files_handler_has_path(values, count, resolved)) {
                free(resolved);
                continue;
            }

            result = files_handler_expand(
                resolved,
                &expanded,
                &expanded_count
            );
            free(resolved);

            if (result != 0) {
                files_handler_free(expanded, expanded_count);
                files_handler_free(values, count);
                return (files_handler_result) {
                    .error = 1,
                    .error_text = "failed to find or read imported source file."
                };
            }

            result = files_handler_append(
                &values,
                &count,
                expanded,
                expanded_count
            );
            files_handler_free(expanded, expanded_count);

            if (result != 0) {
                files_handler_free(values, count);
                return (files_handler_result) {
                    .error = 1,
                    .error_text = "failed to allocate imported source files."
                };
            }
        }

        if (import_result < 0) {
            files_handler_free(values, count);
            return (files_handler_result) {
                .error = 1,
                .error_text = "failed to read imported source path."
            };
        }
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
