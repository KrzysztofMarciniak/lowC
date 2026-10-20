#include "expression.h"

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

static int
semantic_import_targets_file(
    ast *import,
    const char *declaration_file
)
{
    const char *path;
    const char *slash;
    size_t directory_length;
    size_t path_length;
    size_t declaration_length;

    if (
        !import ||
        import->type != AST_IMPORT ||
        !import->import.path ||
        !import->file ||
        !declaration_file
    ) {
        return 0;
    }

    path = import->import.path;
    if (path[0] == '/') {
        return strcmp(path, declaration_file) == 0;
    }

    slash = strrchr(import->file, '/');
    if (!slash) {
        return strcmp(path, declaration_file) == 0;
    }

    directory_length = (size_t)(slash - import->file + 1);
    path_length = strlen(path);
    declaration_length = strlen(declaration_file);

    return declaration_length == directory_length + path_length &&
        memcmp(declaration_file, import->file, directory_length) == 0 &&
        memcmp(
            declaration_file + directory_length,
            path,
            path_length
        ) == 0;
}

int
semantic_declaration_is_visible(
    semantic_context *context,
    const char *requester_file,
    const char *name,
    ast *declaration
)
{
    unsigned long import_index;

    if (!context || !name || !declaration) {
        return 0;
    }

    if (
        (!requester_file && !declaration->file) ||
        (requester_file && declaration->file &&
         strcmp(requester_file, declaration->file) == 0)
    ) {
        return 1;
    }

    if (!requester_file || !context->imports) {
        return 0;
    }

    for (
        import_index = 0;
        import_index < context->imports->count;
        import_index++
    ) {
        ast *import = context->imports->values[import_index];
        unsigned long name_index;

        if (
            !import->file ||
            strcmp(import->file, requester_file) != 0 ||
            !semantic_import_targets_file(import, declaration->file)
        ) {
            continue;
        }

        for (
            name_index = 0;
            name_index < import->import.names.count;
            name_index++
        ) {
            ast *imported_name = import->import.names.values[name_index];

            if (
                imported_name->type == AST_IDENTIFIER &&
                strcmp(imported_name->identifier.name, name) == 0
            ) {
                return 1;
            }
        }
    }

    return 0;
}

int
semantic_type_name_is_visible(
    semantic_context *context,
    const char *requester_file,
    const char *name
)
{
    semantic_struct *structure;

    structure = semantic_struct_find(
        context->structs,
        context->struct_count,
        name
    );

    if (!structure) {
        return 1;
    }

    return semantic_declaration_is_visible(
        context,
        requester_file,
        name,
        structure->declaration
    );
}


/*
 * Return non-zero when the type is one of lowC's builtin
 * integer types.
 *
 * Kept local (rather than exported from type.c) since this is
 * currently the only other place that needs it.
 */
static int
semantic_expression_is_integer(
    const semantic_type *type
)
{
    if (type == NULL)
        return 0;


    switch (type->kind)
    {
        case SEMANTIC_TYPE_U1:
        case SEMANTIC_TYPE_U2:
        case SEMANTIC_TYPE_U4:
        case SEMANTIC_TYPE_U8:
        case SEMANTIC_TYPE_U16:
        case SEMANTIC_TYPE_U32:
        case SEMANTIC_TYPE_U64:

        case SEMANTIC_TYPE_S1:
        case SEMANTIC_TYPE_S2:
        case SEMANTIC_TYPE_S4:
        case SEMANTIC_TYPE_S8:
        case SEMANTIC_TYPE_S16:
        case SEMANTIC_TYPE_S32:
        case SEMANTIC_TYPE_S64:

            return 1;

        default:
            return 0;
    }
}


/*
 * Return non-zero when the expression names a storage location
 * that can be assigned to or have its address taken.
 */
static int
semantic_expression_is_lvalue(
    ast *expression
)
{
    if (expression == NULL)
        return 0;


    switch (expression->type)
    {
        case AST_IDENTIFIER:
        case AST_INDEX:
        case AST_MEMBER:
        case AST_POINTER_MEMBER:

            return 1;


        case AST_UNARY:

            return
                expression->unary.operator ==
                AST_UNARY_DEREFERENCE;


        default:
            return 0;
    }
}


static semantic_type *
semantic_check_binary(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_type *left_type;
    semantic_type *right_type;
    semantic_type *result;


    left_type = semantic_check_expression(
        context,
        expression->binary.left,
        error_text
    );

    if (left_type == NULL)
        return NULL;


    right_type = semantic_check_expression(
        context,
        expression->binary.right,
        error_text
    );

    if (right_type == NULL)
    {
        semantic_type_free(left_type);

        return NULL;
    }


    result = NULL;


    switch (expression->binary.operator)
    {
        /*
         * Pointer arithmetic: `pointer +/- integer` produces a
         * pointer, scaled at code generation time by the size of
         * the pointed-to type. Otherwise both operands must be
         * integers.
         */
        case AST_BINARY_ADD:

            if (
                semantic_expression_is_integer(left_type) &&
                semantic_expression_is_integer(right_type)
            )
                result = semantic_type_builtin("s64");

            else if (
                left_type->kind == SEMANTIC_TYPE_POINTER &&
                semantic_expression_is_integer(right_type)
            )
                result = semantic_type_copy(left_type);

            else if (
                semantic_expression_is_integer(left_type) &&
                right_type->kind == SEMANTIC_TYPE_POINTER
            )
                result = semantic_type_copy(right_type);

            break;


        case AST_BINARY_SUBTRACT:

            if (
                semantic_expression_is_integer(left_type) &&
                semantic_expression_is_integer(right_type)
            )
                result = semantic_type_builtin("s64");

            else if (
                left_type->kind == SEMANTIC_TYPE_POINTER &&
                semantic_expression_is_integer(right_type)
            )
                result = semantic_type_copy(left_type);

            else if (
                left_type->kind == SEMANTIC_TYPE_POINTER &&
                right_type->kind == SEMANTIC_TYPE_POINTER &&
                semantic_type_equal(left_type, right_type)
            )
                /*
                 * Pointer difference, in elements.
                 */
                result = semantic_type_builtin("s64");

            break;


        case AST_BINARY_MULTIPLY:
        case AST_BINARY_DIVIDE:
        case AST_BINARY_MODULO:

            if (
                semantic_expression_is_integer(left_type) &&
                semantic_expression_is_integer(right_type)
            )
                result = semantic_type_builtin("s64");

            break;


        /*
         * Comparisons: either both operands are integers, or they
         * are exactly the same type (e.g. comparing two pointers).
         */
        case AST_BINARY_EQUAL:
        case AST_BINARY_NOT_EQUAL:
        case AST_BINARY_LESS:
        case AST_BINARY_LESS_EQUAL:
        case AST_BINARY_GREATER:
        case AST_BINARY_GREATER_EQUAL:

            if (
                (
                    semantic_expression_is_integer(left_type) &&
                    semantic_expression_is_integer(right_type)
                ) ||
                semantic_type_equal(left_type, right_type)
            )
                result = semantic_type_builtin("u1");

            break;


        case AST_BINARY_AND:
        case AST_BINARY_OR:

            if (
                semantic_expression_is_integer(left_type) &&
                semantic_expression_is_integer(right_type)
            )
                result = semantic_type_builtin("u1");

            break;
    }


    semantic_type_free(left_type);
    semantic_type_free(right_type);


    if (result == NULL)
    {
        *error_text = semantic_strdup(
            "invalid operand types for binary operator"
        );

        return NULL;
    }


    return result;
}


static semantic_type *
semantic_check_unary(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_type *operand_type;
    semantic_type *result;


    switch (expression->unary.operator)
    {
        case AST_UNARY_PLUS:
        case AST_UNARY_MINUS:

            operand_type = semantic_check_expression(
                context,
                expression->unary.operand,
                error_text
            );

            if (operand_type == NULL)
                return NULL;


            if (!semantic_expression_is_integer(operand_type))
            {
                semantic_type_free(operand_type);

                *error_text = semantic_strdup(
                    "operand of unary operator must be an "
                    "integer type"
                );

                return NULL;
            }


            return operand_type;


        case AST_UNARY_NOT:

            operand_type = semantic_check_expression(
                context,
                expression->unary.operand,
                error_text
            );

            if (operand_type == NULL)
                return NULL;


            if (!semantic_expression_is_integer(operand_type))
            {
                semantic_type_free(operand_type);

                *error_text = semantic_strdup(
                    "operand of '!' must be an integer type"
                );

                return NULL;
            }


            semantic_type_free(operand_type);

            result = semantic_type_builtin("u1");

            if (result == NULL)
                *error_text = semantic_strdup("out of memory");

            return result;


        case AST_UNARY_ADDRESS:

            if (
                !semantic_expression_is_lvalue(
                    expression->unary.operand
                )
            )
            {
                *error_text = semantic_strdup(
                    "cannot take the address of a "
                    "non-lvalue expression"
                );

                return NULL;
            }


            operand_type = semantic_check_expression(
                context,
                expression->unary.operand,
                error_text
            );

            if (operand_type == NULL)
                return NULL;


            result = semantic_type_pointer(operand_type);

            if (result == NULL)
            {
                semantic_type_free(operand_type);

                *error_text = semantic_strdup("out of memory");

                return NULL;
            }


            return result;


        case AST_UNARY_DEREFERENCE:

            operand_type = semantic_check_expression(
                context,
                expression->unary.operand,
                error_text
            );

            if (operand_type == NULL)
                return NULL;


            if (operand_type->kind != SEMANTIC_TYPE_POINTER)
            {
                semantic_type_free(operand_type);

                *error_text = semantic_strdup(
                    "cannot dereference a non-pointer type"
                );

                return NULL;
            }


            result = semantic_type_copy(
                operand_type->pointer.element
            );

            semantic_type_free(operand_type);


            if (result == NULL)
            {
                *error_text = semantic_strdup("out of memory");

                return NULL;
            }


            return result;
    }


    *error_text = semantic_strdup("unknown unary operator");

    return NULL;
}


static semantic_type *
semantic_check_index(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_type *object_type;
    semantic_type *index_type;
    semantic_type *result;


    object_type = semantic_check_expression(
        context,
        expression->index.object,
        error_text
    );

    if (object_type == NULL)
        return NULL;


    index_type = semantic_check_expression(
        context,
        expression->index.index,
        error_text
    );

    if (index_type == NULL)
    {
        semantic_type_free(object_type);

        return NULL;
    }


    if (!semantic_expression_is_integer(index_type))
    {
        semantic_type_free(object_type);
        semantic_type_free(index_type);

        *error_text = semantic_strdup(
            "array index must be an integer type"
        );

        return NULL;
    }


    semantic_type_free(index_type);


    if (
        object_type->kind != SEMANTIC_TYPE_POINTER &&
        object_type->kind != SEMANTIC_TYPE_ARRAY
    )
    {
        semantic_type_free(object_type);

        *error_text = semantic_strdup(
            "cannot index a non-pointer, non-array type"
        );

        return NULL;
    }


    result = semantic_type_copy(
        object_type->kind == SEMANTIC_TYPE_POINTER
            ? object_type->pointer.element
            : object_type->array.element
    );

    semantic_type_free(object_type);


    if (result == NULL)
    {
        *error_text = semantic_strdup("out of memory");

        return NULL;
    }


    return result;
}


static semantic_type *
semantic_check_member(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_type *object_type;
    semantic_struct *structure;
    ast *field;
    semantic_type *result;
    const char *name;


    object_type = semantic_check_expression(
        context,
        expression->member.object,
        error_text
    );

    if (object_type == NULL)
        return NULL;


    if (object_type->kind != SEMANTIC_TYPE_STRUCT)
    {
        semantic_type_free(object_type);

        *error_text = semantic_strdup(
            "'.' can only be used on a struct value"
        );

        return NULL;
    }


    structure = semantic_struct_find(
        context->structs,
        context->struct_count,
        object_type->structure.name
    );

    if (structure == NULL)
    {
        semantic_type_free(object_type);

        *error_text = semantic_strdup("unknown struct type");

        return NULL;
    }


    name = expression->member.name;

    field = semantic_struct_find_field(
        structure->declaration,
        name
    );

    semantic_type_free(object_type);


    if (field == NULL)
    {
        *error_text = semantic_strdup(
            "struct has no such field"
        );

        return NULL;
    }


    result = semantic_type_resolve(
        field->struct_field.type,
        field->struct_field.pointer_depth,
        context->structs,
        context->struct_count
    );

    if (result == NULL)
    {
        *error_text = semantic_strdup("unknown field type");

        return NULL;
    }


    return result;
}


static semantic_type *
semantic_check_pointer_member(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_type *object_type;
    semantic_type *element;
    semantic_struct *structure;
    ast *field;
    semantic_type *result;


    object_type = semantic_check_expression(
        context,
        expression->pointer_member.object,
        error_text
    );

    if (object_type == NULL)
        return NULL;


    if (
        object_type->kind != SEMANTIC_TYPE_POINTER ||
        object_type->pointer.element == NULL ||
        object_type->pointer.element->kind != SEMANTIC_TYPE_STRUCT
    )
    {
        semantic_type_free(object_type);

        *error_text = semantic_strdup(
            "'->' can only be used on a pointer to a struct"
        );

        return NULL;
    }


    element = object_type->pointer.element;

    structure = semantic_struct_find(
        context->structs,
        context->struct_count,
        element->structure.name
    );

    if (structure == NULL)
    {
        semantic_type_free(object_type);

        *error_text = semantic_strdup("unknown struct type");

        return NULL;
    }


    field = semantic_struct_find_field(
        structure->declaration,
        expression->pointer_member.name
    );

    semantic_type_free(object_type);


    if (field == NULL)
    {
        *error_text = semantic_strdup(
            "struct has no such field"
        );

        return NULL;
    }


    result = semantic_type_resolve(
        field->struct_field.type,
        field->struct_field.pointer_depth,
        context->structs,
        context->struct_count
    );

    if (result == NULL)
    {
        *error_text = semantic_strdup("unknown field type");

        return NULL;
    }


    return result;
}


static semantic_type *
semantic_check_call(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_function *signature;
    unsigned long i;
    semantic_type *result;


    if (
        expression->call.function == NULL ||
        expression->call.function->type != AST_IDENTIFIER
    )
    {
        *error_text = semantic_strdup(
            "function calls must name a function directly"
        );

        return NULL;
    }


    signature = semantic_function_find(
        context->functions,
        context->function_count,
        expression->call.function->identifier.name
    );

    if (signature == NULL)
    {
        *error_text = semantic_strdup(
            "call to undeclared function"
        );

        return NULL;
    }

    if (
        !semantic_declaration_is_visible(
            context,
            context->current_file,
            signature->name,
            signature->declaration
        )
    ) {
        *error_text = semantic_strdup(
            "function is not imported"
        );

        return NULL;
    }


    if (
        expression->call.arguments.count !=
        signature->parameter_count
    )
    {
        *error_text = semantic_strdup(
            "wrong number of arguments"
        );

        return NULL;
    }


    for (i = 0; i < expression->call.arguments.count; i++)
    {
        semantic_type *argument_type;

        argument_type = semantic_check_expression(
            context,
            expression->call.arguments.values[i],
            error_text
        );

        if (argument_type == NULL)
            return NULL;


        if (
            !semantic_type_assignable(
                signature->parameter_types[i],
                argument_type
            )
        )
        {
            semantic_type_free(argument_type);

            *error_text = semantic_strdup(
                "argument has incompatible type"
            );

            return NULL;
        }


        semantic_type_free(argument_type);
    }


    result = semantic_type_copy(signature->return_type);

    if (result == NULL)
    {
        *error_text = semantic_strdup("out of memory");

        return NULL;
    }


    return result;
}


/*
 * lowC has no assignment statement; `x = 10;` parses as an
 * AST_EXPRESSION_STATEMENT wrapping an AST_ASSIGNMENT expression,
 * so this is the only place an assignment gets checked.
 */
static semantic_type *
semantic_check_assignment(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    semantic_type *left_type;
    semantic_type *right_type;


    if (
        !semantic_expression_is_lvalue(
            expression->assignment.left
        )
    )
    {
        *error_text = semantic_strdup(
            "left-hand side of assignment is not assignable"
        );

        return NULL;
    }


    left_type = semantic_check_expression(
        context,
        expression->assignment.left,
        error_text
    );

    if (left_type == NULL)
        return NULL;


    right_type = semantic_check_expression(
        context,
        expression->assignment.right,
        error_text
    );

    if (right_type == NULL)
    {
        semantic_type_free(left_type);

        return NULL;
    }


    if (!semantic_type_assignable(left_type, right_type))
    {
        semantic_type_free(left_type);
        semantic_type_free(right_type);

        *error_text = semantic_strdup(
            "cannot assign value of incompatible type"
        );

        return NULL;
    }


    semantic_type_free(right_type);

    return left_type;
}


semantic_type *
semantic_check_expression(
    semantic_context *context,
    ast *expression,
    char **error_text
)
{
    switch (expression->type)
    {
        case AST_INTEGER:
        {
            semantic_type *result;

            result = semantic_type_builtin("s64");

            if (result == NULL)
                *error_text = semantic_strdup("out of memory");

            return result;
        }

        case AST_STRING:
        {
            semantic_type *element;
            semantic_type *result;

            element = semantic_type_builtin("u8");
            if (element == NULL)
            {
                *error_text = semantic_strdup("out of memory");
                return NULL;
            }

            result = semantic_type_pointer(element);
            if (result == NULL)
            {
                semantic_type_free(element);
                *error_text = semantic_strdup("out of memory");
            }

            return result;
        }


        case AST_IDENTIFIER:
        {
            semantic_symbol *symbol;
            semantic_type *result;

            symbol = semantic_scope_find(
                context->scope,
                expression->identifier.name
            );

            if (symbol == NULL)
            {
                *error_text = semantic_strdup(
                    "use of undeclared identifier"
                );

                return NULL;
            }

            if (
                !semantic_declaration_is_visible(
                    context,
                    context->current_file,
                    expression->identifier.name,
                    symbol->declaration
                )
            ) {
                *error_text = semantic_strdup(
                    "identifier is not imported"
                );

                return NULL;
            }


            result = semantic_type_copy(symbol->type);

            if (result == NULL)
                *error_text = semantic_strdup("out of memory");

            return result;
        }


        case AST_UNARY:

            return semantic_check_unary(
                context,
                expression,
                error_text
            );


        case AST_BINARY:

            return semantic_check_binary(
                context,
                expression,
                error_text
            );


        case AST_ASSIGNMENT:

            return semantic_check_assignment(
                context,
                expression,
                error_text
            );


        case AST_CALL:

            return semantic_check_call(
                context,
                expression,
                error_text
            );


        case AST_INDEX:

            return semantic_check_index(
                context,
                expression,
                error_text
            );


        case AST_MEMBER:

            return semantic_check_member(
                context,
                expression,
                error_text
            );


        case AST_POINTER_MEMBER:

            return semantic_check_pointer_member(
                context,
                expression,
                error_text
            );


        default:

            *error_text = semantic_strdup(
                "invalid expression"
            );

            return NULL;
    }
}
