#ifndef LOWC_SEMANTIC_SYMBOL_H
#define LOWC_SEMANTIC_SYMBOL_H

#include "type.h"

typedef struct semantic_symbol
{
    char *name;
    semantic_type *type;
    void *declaration;
} semantic_symbol;

semantic_symbol *
semantic_symbol_create(
    const char *name,
    semantic_type *type,
    void *declaration
);

void
semantic_symbol_free(
    semantic_symbol *symbol
);

#endif
