#ifndef LOWC_SEMANTIC_PUBLIC_H
#define LOWC_SEMANTIC_PUBLIC_H

#include "../parser/public.h"

typedef struct semantic_result
{
    int error;

    char *error_text;

    char *file;
    unsigned long line;
    unsigned long column;

    char *source_line;
    char *correct;

    ast *tree;
} semantic_result;

semantic_result
semantic(
    parser_result parser
);

void
semantic_handle_error(
    semantic_result result
);

void
semantic_free(
    semantic_result result
);

#endif
