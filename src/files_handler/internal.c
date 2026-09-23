#define _POSIX_C_SOURCE 200809L
#include "internal.h"


#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int
files_handler_is_source(
    const char *path
)
{
    const char *extension =
        strrchr(path, '.');

    if (!extension)
        return 0;

    return strcmp(extension, ".lowc") == 0;
}

int
files_handler_read(
    source_file *file
)
{
    FILE *stream;
    long size;
    size_t read;

    stream = fopen(file->path, "rb");

    if (!stream)
        return 1;

    if (fseek(stream, 0, SEEK_END) != 0)
    {
        fclose(stream);
        return 1;
    }

    size = ftell(stream);

    if (size < 0)
    {
        fclose(stream);
        return 1;
    }

    rewind(stream);

    file->content =
        malloc((size_t)size + 1);

    if (!file->content)
    {
        fclose(stream);
        return 1;
    }

    read = fread(
        file->content,
        1,
        (size_t)size,
        stream
    );

    fclose(stream);

    if (read != (size_t)size)
    {
        free(file->content);
        file->content = 0;
        return 1;
    }

    file->content[size] = '\0';
    file->size = (unsigned long)size;

    return 0;
}

int
files_handler_expand(
    const char *pattern,
    source_file **files,
    int *count
)
{
    glob_t result;

    *files = 0;
    *count = 0;

    if (glob(pattern, 0, 0, &result) != 0)
        return 1;

    if (result.gl_pathc == 0)
    {
        globfree(&result);
        return 1;
    }

    *files =
        calloc(
            result.gl_pathc,
            sizeof(source_file)
        );

    if (!*files)
    {
        globfree(&result);
        return 1;
    }

    for (size_t index = 0;
         index < result.gl_pathc;
         index++)
    {
        const char *resolved;

resolved = result.gl_pathv[index];

if (!files_handler_is_source(resolved))
        {
            free(*files);
            *files = 0;

            globfree(&result);

            return 2;
        }

        (*files)[index].path =
            malloc(strlen(resolved) + 1);

        if (!(*files)[index].path)
        {
            free(*files);
            *files = 0;

            globfree(&result);

            return 1;
        }

        strcpy(
            (*files)[index].path,
            resolved
        );

        if (files_handler_read(
                &(*files)[index]
            ))
        {
            free((*files)[index].path);
            free(*files);
            *files = 0;

            globfree(&result);

            return 1;
        }
    }

    *count = (int)result.gl_pathc;

    globfree(&result);

    return 0;
}

void
files_handler_free(
    source_file *files,
    int count
)
{
    if (!files)
        return;

    for (int index = 0; index < count; index++)
    {
        free(files[index].path);
        free(files[index].content);
    }

    free(files);
}
