#include "statement.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
semantic_statement_always_returns(
    const ast *statement
)
{
    unsigned long i;

    if (statement == NULL)
        return 0;

    switch (statement->type)
    {
        case AST_RETURN:
            return 1;

        case AST_BLOCK:
            for (i = 0; i < statement->block.statements.count; i++)
            {
                if (
                    semantic_statement_always_returns(
                        statement->block.statements.values[i]
                    )
                )
                    return 1;
            }

            return 0;

        case AST_IF:
            return
                statement->if_statement.else_body != NULL &&
                semantic_statement_always_returns(
                    statement->if_statement.then_body
                ) &&
                semantic_statement_always_returns(
                    statement->if_statement.else_body
                );

        default:
            return 0;
    }
}


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

static int
semantic_is_zero_initializer(
    const ast *expression
)
{
    return
        expression != NULL &&
        expression->type == AST_INTEGER &&
        strtoull(expression->integer.value, NULL, 10) == 0;
}

static int
semantic_asm_is_register(
    const char *name
)
{
    static const char *registers[] = {
        "rax", "rbx", "rcx", "rdx",
        "rsi", "rdi", "r8", "r9",
        "r10", "r11", "r12", "r13",
        "r14", "r15", "rsp", "rbp"
    };

    for (unsigned long index = 0;
         index < sizeof(registers) / sizeof(registers[0]);
         index++) {
        if (strcmp(name, registers[index]) == 0)
            return 1;
    }

    return 0;
}

static int
semantic_asm_is_forbidden_register(
    const char *name
)
{
    return strcmp(name, "rsp") == 0 || strcmp(name, "rbp") == 0;
}

static int
semantic_asm_is_lvalue(
    ast *expression
)
{
    if (!expression)
        return 0;

    switch (expression->type) {
        case AST_IDENTIFIER:
        case AST_INDEX:
        case AST_MEMBER:
        case AST_POINTER_MEMBER:
            return 1;

        case AST_UNARY:
            return expression->unary.operator == AST_UNARY_DEREFERENCE;

        default:
            return 0;
    }
}

static int
semantic_asm_type_supported(
    const semantic_type *type
)
{
    if (!type)
        return 0;

    if (type->kind == SEMANTIC_TYPE_POINTER)
        return 1;

    switch (type->kind) {
        case SEMANTIC_TYPE_U1:
        case SEMANTIC_TYPE_U2:
        case SEMANTIC_TYPE_U4:
        case SEMANTIC_TYPE_U8:
        case SEMANTIC_TYPE_S1:
        case SEMANTIC_TYPE_S2:
        case SEMANTIC_TYPE_S4:
        case SEMANTIC_TYPE_S8:
            return 1;

        default:
            return 0;
    }
}

static int
semantic_check_assembly(
    semantic_context *context,
    ast *assembly,
    char **error_text
)
{
    unsigned long output_count = assembly->assembly.outputs.count;
    unsigned long input_count = assembly->assembly.inputs.count;
    unsigned long operand_count = output_count + input_count;

    for (unsigned long index = 0; index < output_count; index++) {
        ast *operand = assembly->assembly.outputs.values[index];
        const char *constraint = operand->assembly_operand.constraint;
        const char *register_name = constraint && constraint[0] == '='
            ? constraint + 1
            : "";
        semantic_type *type;

        if (
            constraint == NULL ||
            constraint[0] != '=' ||
            !semantic_asm_is_register(register_name) ||
            semantic_asm_is_forbidden_register(register_name)
        ) {
            *error_text = semantic_strdup(
                "invalid inline assembly output register constraint"
            );
            return 1;
        }

        if (!semantic_asm_is_lvalue(operand->assembly_operand.expression)) {
            *error_text = semantic_strdup(
                "inline assembly output must be an assignable lvalue"
            );
            return 1;
        }

        for (unsigned long previous = 0; previous < index; previous++) {
            const char *previous_constraint =
                assembly->assembly.outputs.values[previous]
                    ->assembly_operand.constraint;

            if (strcmp(register_name, previous_constraint + 1) == 0) {
                *error_text = semantic_strdup(
                    "inline assembly output registers must be unique"
                );
                return 1;
            }
        }

        type = semantic_check_expression(
            context,
            operand->assembly_operand.expression,
            error_text
        );
        if (type == NULL)
            return 1;

        if (!semantic_asm_type_supported(type)) {
            semantic_type_free(type);
            *error_text = semantic_strdup(
                "inline assembly outputs require integer or pointer values"
            );
            return 1;
        }

        semantic_type_free(type);
    }

    for (unsigned long index = 0; index < input_count; index++) {
        ast *operand = assembly->assembly.inputs.values[index];
        const char *register_name =
            operand->assembly_operand.constraint;
        semantic_type *type;

        if (
            !semantic_asm_is_register(register_name) ||
            semantic_asm_is_forbidden_register(register_name)
        ) {
            *error_text = semantic_strdup(
                "invalid inline assembly input register constraint"
            );
            return 1;
        }

        for (unsigned long previous = 0; previous < index; previous++) {
            if (strcmp(
                    register_name,
                    assembly->assembly.inputs.values[previous]
                        ->assembly_operand.constraint
                ) == 0) {
                *error_text = semantic_strdup(
                    "inline assembly input registers must be unique"
                );
                return 1;
            }
        }

        type = semantic_check_expression(
            context,
            operand->assembly_operand.expression,
            error_text
        );
        if (type == NULL)
            return 1;

        if (!semantic_asm_type_supported(type)) {
            semantic_type_free(type);
            *error_text = semantic_strdup(
                "inline assembly inputs require integer or pointer values"
            );
            return 1;
        }

        semantic_type_free(type);
    }

    for (
        unsigned long index = 0;
        index < assembly->assembly.clobbers.count;
        index++
    ) {
        const char *clobber =
            assembly->assembly.clobbers.values[index]->string.value;

        if (
            strcmp(clobber, "memory") != 0 &&
            strcmp(clobber, "cc") != 0 &&
            (
                !semantic_asm_is_register(clobber) ||
                semantic_asm_is_forbidden_register(clobber)
            )
        ) {
            *error_text = semantic_strdup(
                "invalid inline assembly clobber"
            );
            return 1;
        }

        for (unsigned long previous = 0; previous < index; previous++) {
            if (strcmp(
                    clobber,
                    assembly->assembly.clobbers.values[previous]
                        ->string.value
                ) == 0) {
                *error_text = semantic_strdup(
                    "inline assembly clobbers must be unique"
                );
                return 1;
            }
        }
    }

    for (
        unsigned long index = 0;
        assembly->assembly.template[index] != '\0';
        index++
    ) {
        unsigned long placeholder = 0;

        if (assembly->assembly.template[index] != '%')
            continue;

        index++;
        if (assembly->assembly.template[index] == '%')
            continue;

        if (
            assembly->assembly.template[index] < '0' ||
            assembly->assembly.template[index] > '9'
        ) {
            *error_text = semantic_strdup(
                "inline assembly '%' must start a numbered operand or be escaped as '%%'"
            );
            return 1;
        }

        do {
            placeholder =
                placeholder * 10 +
                (unsigned long)(assembly->assembly.template[index] - '0');
            index++;
        } while (
            assembly->assembly.template[index] >= '0' &&
            assembly->assembly.template[index] <= '9'
        );
        index--;

        if (placeholder >= operand_count) {
            *error_text = semantic_strdup(
                "inline assembly operand placeholder is out of range"
            );
            return 1;
        }
    }

    return 0;
}

static int
semantic_check_multi_assignment(
    semantic_context *context,
    ast *statement,
    char **error_text
)
{
    semantic_function *signature;
    unsigned long index;

    if (
        statement->multi_assignment.value == NULL ||
        statement->multi_assignment.value->type != AST_CALL
    ) {
        *error_text = semantic_strdup(
            "tuple assignment requires a function call"
        );
        return 1;
    }

    signature = semantic_check_call_signature(
        context,
        statement->multi_assignment.value,
        error_text
    );
    if (signature == NULL)
        return 1;

    if (
        signature->return_type_count !=
        statement->multi_assignment.targets.count
    ) {
        *error_text = semantic_strdup(
            "tuple assignment has the wrong number of targets"
        );
        return 1;
    }

    for (
        index = 0;
        index < statement->multi_assignment.targets.count;
        index++
    ) {
        ast *target = statement->multi_assignment.targets.values[index];
        semantic_type *target_type;

        if (!semantic_expression_is_lvalue(target)) {
            *error_text = semantic_strdup(
                "tuple assignment target is not assignable"
            );
            return 1;
        }

        target_type = semantic_check_expression(
            context,
            target,
            error_text
        );
        if (target_type == NULL)
            return 1;

        if (!semantic_type_assignable(
                target_type,
                signature->return_types[index]
            )) {
            semantic_type_free(target_type);
            *error_text = semantic_strdup(
                "tuple assignment target has incompatible type"
            );
            return 1;
        }

        semantic_type_free(target_type);
    }

    return 0;
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
        case AST_MULTI_ASSIGNMENT:
            return semantic_check_multi_assignment(
                context,
                statement,
                error_text
            );

        case AST_VARIABLE_DECLARATION:
        {
            semantic_symbol *symbol;

            if (
                !semantic_type_name_is_visible(
                    context,
                    statement->file,
                    statement->variable.type
                )
            ) {
                *error_text = semantic_strdup(
                    "variable type is not imported"
                );

                return 1;
            }


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
             *     s8 values[5];
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

            if (statement->variable.value == NULL)
            {
                *error_text = semantic_strdup(
                    "variables must have an initializer"
                );

                semantic_type_free(type);
                return 1;
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

                if (
                    (
                        type->kind == SEMANTIC_TYPE_ARRAY ||
                        type->kind == SEMANTIC_TYPE_STRUCT
                    ) &&
                    semantic_is_zero_initializer(
                        statement->variable.value
                    )
                ) {
                    return 0;
                }

                if (
                    type->kind == SEMANTIC_TYPE_POINTER &&
                    semantic_is_zero_initializer(
                        statement->variable.value
                    )
                ) {
                    return 0;
                }

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
            if (context->current_function != NULL)
            {
                unsigned long value_count =
                    statement->return_statement.values.count;

                if (value_count == 0) {
                    if (
                        context->current_function->return_type_count == 1 &&
                        context->current_function->return_types[0]->kind ==
                            SEMANTIC_TYPE_VOID
                    ) {
                        return 0;
                    }
                    *error_text = semantic_strdup(
                        "missing return value"
                    );
                    return 1;
                }

                if (
                    value_count !=
                    context->current_function->return_type_count
                ) {
                    *error_text = semantic_strdup(
                        "return statement has the wrong number of values"
                    );
                    return 1;
                }

                for (i = 0; i < value_count; i++) {
                    semantic_type *value_type = semantic_check_expression(
                        context,
                        statement->return_statement.values.values[i],
                        error_text
                    );

                    if (value_type == NULL)
                        return 1;

                    if (!semantic_type_assignable(
                            context->current_function->return_types[i],
                            value_type
                        )) {
                        semantic_type_free(value_type);
                        *error_text = semantic_strdup(
                            "return value has incompatible type"
                        );
                        return 1;
                    }

                    semantic_type_free(value_type);
                }
            }

            return 0;
        }

        case AST_ASM:
            return semantic_check_assembly(
                context,
                statement,
                error_text
            );


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
