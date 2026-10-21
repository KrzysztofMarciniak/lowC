#ifndef LOWC_FILES_HANDLER_PUBLIC_H
#define LOWC_FILES_HANDLER_PUBLIC_H

#include "main_arg_handler/public.h"

typedef struct source_file
{
    char *path;
    char *content;
    unsigned long size;
    int imported;
} source_file;

typedef struct files_handler_result
{
    int error;
    char *error_text;

    int count;
    source_file *values;
} files_handler_result;

files_handler_result
files_handler(
    files *files
);

void
files_handler_handle_error(
    files_handler_result result
);

#endif
