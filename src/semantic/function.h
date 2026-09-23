#ifndef LOWC_SEMANTIC_FUNCTION_H
#define LOWC_SEMANTIC_FUNCTION_H

#include "type.h"
#include "struct.h"
#include "../parser/public.h"

typedef struct semantic_function
{
    char *name;

    semantic_type *return_type;

    semantic_type **parameter_types;
    unsigned long parameter_count;

    ast *declaration;
} semantic_function;

/**
 * @brief Build a function signature from its AST declaration.
 *
 * The struct list must already be registered (structs don't depend
 * on functions, so this should run after struct registration but
 * doesn't itself depend on other functions being registered yet).
 *
 * @return A newly allocated signature, or NULL on an unknown
 *         parameter/return type or allocation failure.
 */
semantic_function *
semantic_function_create(
    ast *function,
    semantic_struct **structs,
    unsigned long struct_count
);

semantic_function *
semantic_function_find(
    semantic_function **functions,
    unsigned long count,
    const char *name
);

void
semantic_function_free(
    semantic_function *function
);

#endif
