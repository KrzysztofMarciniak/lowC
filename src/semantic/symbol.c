#include "symbol.h"

#include <stdlib.h>
#include <string.h>

semantic_symbol *
semantic_symbol_create(
    const char *name,
    semantic_type *type,
    void *declaration
)
{
    semantic_symbol *symbol;

    symbol = malloc(sizeof(semantic_symbol));

    if (symbol == NULL)
        return NULL;

    symbol->name = malloc(strlen(name) + 1);

    if (symbol->name == NULL)
    {
        free(symbol);
        return NULL;
    }

    strcpy(symbol->name, name);

    symbol->type = type;
    symbol->declaration = declaration;

    return symbol;
}

void
semantic_symbol_free(
    semantic_symbol *symbol
)
{
    if (symbol == NULL)
        return;

    free(symbol->name);
    semantic_type_free(symbol->type);
    free(symbol);
}
