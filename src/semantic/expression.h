#ifndef LOWC_SEMANTIC_EXPRESSION_H
#define LOWC_SEMANTIC_EXPRESSION_H

#include "scope.h"
#include "function.h"
#include "struct.h"

typedef struct semantic_context
{
    semantic_scope *scope;
    ast_list *imports;

    semantic_function **functions;
    unsigned long function_count;

    semantic_struct **structs;
    unsigned long struct_count;

    /*
     * The signature of the function currently being checked (NULL
     * outside of a function body, e.g. while checking a global
     * initializer). Kept as the resolved semantic_function rather
     * than the raw ast node so its resolved return_types (which
     * already account for pointer depth and struct types) can be used
     * directly by statement checking, instead of being rebuilt from
     * scratch.
     */
    semantic_function *current_function;
    const char *current_file;
} semantic_context;

int
semantic_declaration_is_visible(
    semantic_context *context,
    const char *requester_file,
    const char *name,
    ast *declaration
);

int
semantic_type_name_is_visible(
    semantic_context *context,
    const char *requester_file,
    const char *name
);

int
semantic_expression_is_lvalue(
    ast *expression
);

semantic_function *
semantic_check_call_signature(
    semantic_context *context,
    ast *expression,
    char **error_text
);

semantic_type *
semantic_check_expression(
    semantic_context *context,
    ast *expression,
    char **error_text
);

#endif
