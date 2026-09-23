#ifndef LOWC_SEMANTIC_SCOPE_H
#define LOWC_SEMANTIC_SCOPE_H

#include "symbol.h"

typedef struct semantic_scope semantic_scope;

struct semantic_scope
{
    semantic_symbol **symbols;
    unsigned long count;

    semantic_scope *parent;
};

semantic_scope *
semantic_scope_create(
    semantic_scope *parent
);

int
semantic_scope_add(
    semantic_scope *scope,
    semantic_symbol *symbol
);

semantic_symbol *
semantic_scope_find(
    semantic_scope *scope,
    const char *name
);

semantic_symbol *
semantic_scope_find_local(
    semantic_scope *scope,
    const char *name
);

void
semantic_scope_free(
    semantic_scope *scope
);

#endif
