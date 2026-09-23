#include "scope.h"

#include <stdlib.h>
#include <string.h>

semantic_scope *
semantic_scope_create(
    semantic_scope *parent
)
{
    semantic_scope *scope;

    scope = calloc(1, sizeof(semantic_scope));

    if (scope == NULL)
        return NULL;

    scope->parent = parent;

    return scope;
}

int
semantic_scope_add(
    semantic_scope *scope,
    semantic_symbol *symbol
)
{
    semantic_symbol **symbols;

    if (semantic_scope_find_local(scope, symbol->name) != NULL)
        return 1;

    symbols = realloc(
        scope->symbols,
        sizeof(semantic_symbol *) * (scope->count + 1)
    );

    if (symbols == NULL)
        return 1;

    scope->symbols = symbols;
    scope->symbols[scope->count] = symbol;
    scope->count++;

    return 0;
}

semantic_symbol *
semantic_scope_find_local(
    semantic_scope *scope,
    const char *name
)
{
    unsigned long i;

    if (scope == NULL)
        return NULL;

    for (i = 0; i < scope->count; i++)
    {
        if (strcmp(scope->symbols[i]->name, name) == 0)
            return scope->symbols[i];
    }

    return NULL;
}

semantic_symbol *
semantic_scope_find(
    semantic_scope *scope,
    const char *name
)
{
    semantic_symbol *symbol;

    while (scope != NULL)
    {
        symbol = semantic_scope_find_local(scope, name);

        if (symbol != NULL)
            return symbol;

        scope = scope->parent;
    }

    return NULL;
}

void
semantic_scope_free(
    semantic_scope *scope
)
{
    unsigned long i;

    if (scope == NULL)
        return;

    for (i = 0; i < scope->count; i++)
        semantic_symbol_free(scope->symbols[i]);

    free(scope->symbols);
    free(scope);
}
