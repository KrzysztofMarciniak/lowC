#ifndef LOWC_FILES_HANDLER_INTERNAL_H
#define LOWC_FILES_HANDLER_INTERNAL_H

#include "public.h"

int
files_handler_expand(
    const char *pattern,
    source_file **files,
    int *count
);

int
files_handler_read(
    source_file *file
);

int
files_handler_is_source(
    const char *path
);

void
files_handler_free(
    source_file *files,
    int count
);

#endif
