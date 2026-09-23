#ifndef LOWC_SEMANTIC_EXPRESSION_H
#define LOWC_SEMANTIC_EXPRESSION_H

#include "scope.h"
#include "function.h"
#include "struct.h"

typedef struct semantic_context
{
    semantic_scope *scope;

    semantic_function **functions;
    unsigned long function_count;

    semantic_struct **structs;
    unsigned long struct_count;

    /*
     * The signature of the function currently being checked (NULL
     * outside of a function body, e.g. while checking a global
     * initializer). Kept as the resolved semantic_function rather
     * than the raw ast node so its return_type (which already
     * accounts for pointer depth and struct types) can be used
     * directly by statement checking, instead of being rebuilt from
     * scratch.
     */
    semantic_function *current_function;
} semantic_context;

semantic_type *
semantic_check_expression(
    semantic_context *context,
    ast *expression,
    char **error_text
);

#endif
