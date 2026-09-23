#include "public.h"

#include "error.h"
#include "statement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char *
semantic_strdup(
    const char *value
)
{
    char *copy;

    if (value == NULL)
        return NULL;

    copy = malloc(strlen(value) + 1);

    if (copy == NULL)
        return NULL;

    strcpy(copy, value);

    return copy;
}


/**
 * @brief Check one function body against its already-registered signature.
 *
 * Creates a child scope for the function's parameters and locals so
 * that body statements can see globals (via the parent chain) as
 * well as their own parameters, checks the body, then restores the
 * previous scope and current-function.
 *
 * @return 0 on success, 1 on error (with *error_text set).
 */
static int
semantic_check_function(
    semantic_context *context,
    ast *function,
    semantic_function *signature,
    char **error_text
)
{
    semantic_scope *previous_scope;
    semantic_function *previous_function;
    unsigned long i;

    previous_scope = context->scope;
    previous_function = context->current_function;

    context->scope = semantic_scope_create(previous_scope);

    if (context->scope == NULL)
    {
        context->scope = previous_scope;

        *error_text = semantic_strdup(
            "failed to create function scope"
        );

        return 1;
    }

    context->current_function = signature;

    for (i = 0; i < function->function.parameters.count; i++)
    {
        ast *parameter;
        semantic_type *type;
        semantic_symbol *symbol;

        parameter = function->function.parameters.values[i];

        type = semantic_type_resolve(
            parameter->parameter.type,
            parameter->parameter.pointer_depth,
            context->structs,
            context->struct_count
        );

        if (type == NULL)
        {
            *error_text = semantic_strdup(
                "unknown parameter type"
            );

            goto fail;
        }

        if (
            semantic_scope_find_local(
                context->scope,
                parameter->parameter.name
            ) != NULL
        )
        {
            semantic_type_free(type);

            *error_text = semantic_strdup(
                "duplicate parameter"
            );

            goto fail;
        }

        symbol = semantic_symbol_create(
            parameter->parameter.name,
            type,
            parameter
        );

        if (symbol == NULL)
        {
            semantic_type_free(type);

            *error_text = semantic_strdup(
                "out of memory"
            );

            goto fail;
        }

        if (semantic_scope_add(context->scope, symbol))
        {
            semantic_symbol_free(symbol);

            *error_text = semantic_strdup(
                "duplicate parameter"
            );

            goto fail;
        }
    }

    if (
        semantic_check_statement(
            context,
            function->function.body,
            error_text
        )
    )
        goto fail;

    semantic_scope_free(context->scope);

    context->scope = previous_scope;
    context->current_function = previous_function;

    return 0;

fail:
    semantic_scope_free(context->scope);

    context->scope = previous_scope;
    context->current_function = previous_function;

    return 1;
}


semantic_result
semantic(
    parser_result parser
)
{
    semantic_result result;
    semantic_context context;
    unsigned long i;
    char *error_text;

    memset(&result, 0, sizeof(result));
    memset(&context, 0, sizeof(context));

    if (parser.error)
    {
        result.error = 1;
        result.error_text = parser.error_text;
        result.file = parser.file;
        result.line = parser.line;
        result.column = parser.column;

        return result;
    }

    context.scope = semantic_scope_create(NULL);

    if (context.scope == NULL)
    {
        result.error = 1;
        result.error_text = semantic_strdup(
            "failed to create global scope"
        );

        return result;
    }

    /*
     * Pass 1: register struct declarations first, so later type
     * resolution (parameters, return types, variables) can refer to
     * them regardless of source order.
     */
    for (i = 0; i < parser.tree->program.structs.count; i++)
    {
        ast *declaration;
        semantic_struct *structure;
        semantic_struct **structs;

        declaration = parser.tree->program.structs.values[i];

        structure = semantic_struct_create(declaration);

        if (structure == NULL)
        {
            result = semantic_error(
                declaration,
                "failed to register struct"
            );

            goto cleanup;
        }

        structs = realloc(
            context.structs,
            sizeof(semantic_struct *) *
                (context.struct_count + 1)
        );

        if (structs == NULL)
        {
            semantic_struct_free(structure);

            result = semantic_error(
                declaration,
                "out of memory"
            );

            goto cleanup;
        }

        context.structs = structs;
        context.structs[context.struct_count] = structure;
        context.struct_count++;
    }

    /*
     * Pass 2: register every function's signature before checking
     * any body, so calls to functions declared later in the file
     * (including recursive/mutually-recursive calls) resolve.
     */
    for (i = 0; i < parser.tree->program.functions.count; i++)
    {
        ast *function;
        semantic_function *signature;
        semantic_function **functions;

        function = parser.tree->program.functions.values[i];

        signature = semantic_function_create(
            function,
            context.structs,
            context.struct_count
        );

        if (signature == NULL)
        {
            result = semantic_error(
                function,
                "failed to resolve function signature"
            );

            goto cleanup;
        }

        functions = realloc(
            context.functions,
            sizeof(semantic_function *) *
                (context.function_count + 1)
        );

        if (functions == NULL)
        {
            semantic_function_free(signature);

            result = semantic_error(
                function,
                "out of memory"
            );

            goto cleanup;
        }

        context.functions = functions;
        context.functions[context.function_count] = signature;
        context.function_count++;
    }

    /*
     * Pass 3: register global variables in the top-level scope.
     */
    for (i = 0; i < parser.tree->program.globals.count; i++)
    {
        ast *node;
        semantic_type *type;
        semantic_symbol *symbol;

        node = parser.tree->program.globals.values[i];

        type = semantic_type_resolve(
            node->variable.type,
            node->variable.pointer_depth,
            context.structs,
            context.struct_count
        );

        if (type == NULL)
        {
            result = semantic_error(
                node,
                "unknown type for global variable"
            );

            goto cleanup;
        }

        if (
            semantic_scope_find_local(
                context.scope,
                node->variable.name
            ) != NULL
        )
        {
            semantic_type_free(type);

            result = semantic_error(
                node,
                "duplicate global variable"
            );

            goto cleanup;
        }

        symbol = semantic_symbol_create(
            node->variable.name,
            type,
            node
        );

        if (symbol == NULL)
        {
            semantic_type_free(type);

            result = semantic_error(
                node,
                "out of memory"
            );

            goto cleanup;
        }

        if (semantic_scope_add(context.scope, symbol))
        {
            semantic_symbol_free(symbol);

            result = semantic_error(
                node,
                "failed to register global variable"
            );

            goto cleanup;
        }
    }

    /*
     * Pass 4: check global initializers. This happens after every
     * global is registered, so an initializer may reference a
     * global declared later in the source.
     */
    for (i = 0; i < parser.tree->program.globals.count; i++)
    {
        ast *node;
        semantic_symbol *symbol;
        semantic_type *value;

        node = parser.tree->program.globals.values[i];

        if (node->variable.value == NULL)
            continue;

        symbol = semantic_scope_find_local(
            context.scope,
            node->variable.name
        );

        error_text = NULL;

        value = semantic_check_expression(
            &context,
            node->variable.value,
            &error_text
        );

        if (value == NULL)
        {
            result.error = 1;
            result.error_text = error_text;
            result.file = node->file;
            result.line = node->line;
            result.column = node->column;

            goto cleanup;
        }

        if (!semantic_type_assignable(symbol->type, value))
        {
            semantic_type_free(value);

            result = semantic_error(
                node,
                "global initializer has incompatible type"
            );

            goto cleanup;
        }

        semantic_type_free(value);
    }

    /*
     * Pass 5: check every function body.
     */
    for (i = 0; i < parser.tree->program.functions.count; i++)
    {
        ast *function;

        function = parser.tree->program.functions.values[i];

        error_text = NULL;

        if (
            semantic_check_function(
                &context,
                function,
                context.functions[i],
                &error_text
            )
        )
        {
            result.error = 1;
            result.error_text = error_text;
            result.file = function->file;
            result.line = function->line;
            result.column = function->column;

            goto cleanup;
        }
    }

    result.tree = parser.tree;

cleanup:

    for (i = 0; i < context.function_count; i++)
        semantic_function_free(context.functions[i]);

    free(context.functions);

    for (i = 0; i < context.struct_count; i++)
        semantic_struct_free(context.structs[i]);

    free(context.structs);

    /*
     * Frees every symbol registered in the top-level scope too,
     * including global variables.
     */
    semantic_scope_free(context.scope);

    return result;
}

void
semantic_handle_error(
    semantic_result result
)
{
    if (!result.error)
        return;

    fprintf(
        stderr,
        "semantic error: %s\n",
        result.error_text != NULL
            ? result.error_text
            : "unknown semantic error."
    );

    if (result.file != NULL)
    {
        fprintf(
            stderr,
            "  --> %s:%lu:%lu\n",
            result.file,
            result.line,
            result.column
        );
    }
}

void
semantic_free(
    semantic_result result
)
{
    free(result.error_text);
}
