#include "statement.h"

#include <stdlib.h>
#include <stdio.h>
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


int
semantic_check_statement(
    semantic_context *context,
    ast *statement,
    char **error_text
)
{
    unsigned long i;
    semantic_type *type;


    switch (statement->type)
    {
        case AST_VARIABLE_DECLARATION:
        {
            semantic_symbol *symbol;


            type = semantic_type_resolve(
                statement->variable.type,
                statement->variable.pointer_depth,
                context->structs,
                context->struct_count
            );


            if (type == NULL)
            {
                *error_text = semantic_strdup(
                    "unknown variable type"
                );

                return 1;
            }


            /*
             * Convert an array declaration such as:
             *
             *     s64 values[5];
             *
             * from:
             *
             *     S64
             *
             * into:
             *
             *     ARRAY[5] of S64
             *
             * before putting the variable into the symbol table.
             */
            if (statement->variable.array_length != NULL)
            {
                unsigned long count;
                semantic_type *array_type;


                if (
                    statement->variable.array_length->type !=
                    AST_INTEGER
                )
                {
                    semantic_type_free(type);

                    *error_text = semantic_strdup(
                        "array length must be an integer"
                    );

                    return 1;
                }


                count = strtoul(
                    statement->variable.array_length->integer.value,
                    NULL,
                    10
                );


                array_type = semantic_type_array(
                    type,
                    count
                );


                if (array_type == NULL)
                {
                    semantic_type_free(type);

                    *error_text = semantic_strdup(
                        "out of memory"
                    );

                    return 1;
                }


                /*
                 * semantic_type_array() takes ownership of
                 * `type` when it succeeds.
                 */
                type = array_type;
            }


            if (
                semantic_scope_find_local(
                    context->scope,
                    statement->variable.name
                ) != NULL
            )
            {
                *error_text = semantic_strdup(
                    "duplicate declaration"
                );

                semantic_type_free(type);

                return 1;
            }


            symbol = semantic_symbol_create(
                statement->variable.name,
                type,
                statement
            );


            if (symbol == NULL)
            {
                semantic_type_free(type);

                *error_text = semantic_strdup(
                    "out of memory"
                );

                return 1;
            }


            if (
                semantic_scope_add(
                    context->scope,
                    symbol
                )
            )
            {
                semantic_symbol_free(symbol);

                *error_text = semantic_strdup(
                    "duplicate declaration"
                );

                return 1;
            }


            if (statement->variable.value != NULL)
            {
                semantic_type *value;


                value = semantic_check_expression(
                    context,
                    statement->variable.value,
                    error_text
                );


                if (value == NULL)
                    return 1;


                if (
                    !semantic_type_assignable(
                        symbol->type,
                        value
                    )
                )
                {
                    semantic_type_free(value);

                    *error_text = semantic_strdup(
                        "initializer has incompatible type"
                    );

                    return 1;
                }


                semantic_type_free(value);
            }


            return 0;
        }


        case AST_EXPRESSION_STATEMENT:

            type = semantic_check_expression(
                context,
                statement->expression_statement.expression,
                error_text
            );


            if (type == NULL)
                return 1;


            semantic_type_free(type);

            return 0;


        case AST_RETURN:
        {
            semantic_type *value_type;


            value_type = NULL;


            if (statement->return_statement.value != NULL)
            {
                value_type = semantic_check_expression(
                    context,
                    statement->return_statement.value,
                    error_text
                );


                if (value_type == NULL)
                    return 1;
            }


            if (context->current_function != NULL)
            {
                semantic_type *expected;


                expected = context->current_function->return_type;


                if (value_type == NULL)
                {
                    /*
                     * A bare `return;` is only valid in a function
                     * that returns void.
                     */
                    if (
                        expected == NULL ||
                        expected->kind != SEMANTIC_TYPE_VOID
                    )
                    {
                        *error_text = semantic_strdup(
                            "missing return value"
                        );

                        return 1;
                    }
                }
                else if (
                    !semantic_type_assignable(
                        expected,
                        value_type
                    )
                )
                {
                    semantic_type_free(value_type);

                    *error_text = semantic_strdup(
                        "return value has incompatible type"
                    );

                    return 1;
                }
            }


            semantic_type_free(value_type);

            return 0;
        }


        case AST_BLOCK:

            for (
                i = 0;
                i < statement->block.statements.count;
                i++
            )
            {
                if (
                    semantic_check_statement(
                        context,
                        statement->block.statements.values[i],
                        error_text
                    )
                )
                    return 1;
            }

            return 0;


        case AST_IF:

            type = semantic_check_expression(
                context,
                statement->if_statement.condition,
                error_text
            );


            if (type == NULL)
                return 1;


            semantic_type_free(type);


            if (
                semantic_check_statement(
                    context,
                    statement->if_statement.then_body,
                    error_text
                )
            )
                return 1;


            if (
                statement->if_statement.else_body != NULL &&
                semantic_check_statement(
                    context,
                    statement->if_statement.else_body,
                    error_text
                )
            )
                return 1;


            return 0;


        case AST_FOR:

            if (
                statement->for_statement.init != NULL &&
                semantic_check_statement(
                    context,
                    statement->for_statement.init,
                    error_text
                )
            )
                return 1;


            if (statement->for_statement.condition != NULL)
            {
                type = semantic_check_expression(
                    context,
                    statement->for_statement.condition,
                    error_text
                );


                if (type == NULL)
                    return 1;


                semantic_type_free(type);
            }


            if (statement->for_statement.increment != NULL)
            {
                type = semantic_check_expression(
                    context,
                    statement->for_statement.increment,
                    error_text
                );


                if (type == NULL)
                    return 1;


                semantic_type_free(type);
            }


            return semantic_check_statement(
                context,
                statement->for_statement.body,
                error_text
            );


        default:
            return 0;
    }
}
