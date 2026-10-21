#include "public.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct ir_context
{
    ast *program;
    ir_module *module;
} ir_context;

static int
ir_list_append(
    ast_list *list,
    ast *value
)
{
    ast **values;

    values = realloc(
        list->values,
        sizeof(*values) * (size_t)(list->count + 1)
    );
    if (!values) {
        return 1;
    }

    list->values = values;
    list->values[list->count++] = value;
    return 0;
}

static int
ir_list_contains(
    const ast_list *list,
    const ast *value
)
{
    for (unsigned long index = 0; index < list->count; index++) {
        if (list->values[index] == value) {
            return 1;
        }
    }

    return 0;
}

static ast *
ir_find_declaration(
    const ast_list *declarations,
    const char *name
)
{
    if (!name) {
        return NULL;
    }

    for (unsigned long index = 0; index < declarations->count; index++) {
        ast *declaration = declarations->values[index];
        const char *declaration_name = NULL;

        if (declaration->type == AST_FUNCTION) {
            declaration_name = declaration->function.name;
        } else if (
            declaration->type == AST_GLOBAL ||
            declaration->type == AST_VARIABLE_DECLARATION
        ) {
            declaration_name = declaration->variable.name;
        } else if (declaration->type == AST_STRUCT) {
            declaration_name = declaration->structure.name;
        }

        if (
            declaration_name != NULL &&
            strcmp(declaration_name, name) == 0
        ) {
            return declaration;
        }
    }

    return NULL;
}

static int ir_mark_declaration(ir_context *context, ast *declaration);

static int
ir_constant_expression(
    const ast *expression,
    uint64_t *value
)
{
    uint64_t left;
    uint64_t right;
    char *end;
    long long integer;

    if (!expression || !value) {
        return 0;
    }

    switch (expression->type) {
        case AST_INTEGER:
            errno = 0;
            integer = strtoll(expression->integer.value, &end, 10);
            if (errno != 0 || *end != '\0') {
                return 0;
            }
            *value = (uint64_t)integer;
            return 1;

        case AST_UNARY:
            if (!ir_constant_expression(
                    expression->unary.operand,
                    &left
                )) {
                return 0;
            }

            switch (expression->unary.operator) {
                case AST_UNARY_PLUS:
                    *value = left;
                    return 1;
                case AST_UNARY_MINUS:
                    *value = UINT64_C(0) - left;
                    return 1;
                case AST_UNARY_NOT:
                    *value = left == 0;
                    return 1;
                default:
                    return 0;
            }

        case AST_BINARY:
            if (!ir_constant_expression(
                    expression->binary.left,
                    &left
                )) {
                return 0;
            }

            if (expression->binary.operator == AST_BINARY_AND && left == 0) {
                *value = 0;
                return 1;
            }
            if (expression->binary.operator == AST_BINARY_OR && left != 0) {
                *value = 1;
                return 1;
            }

            if (!ir_constant_expression(
                    expression->binary.right,
                    &right
                )) {
                return 0;
            }

            switch (expression->binary.operator) {
                case AST_BINARY_ADD:
                    *value = left + right;
                    return 1;
                case AST_BINARY_SUBTRACT:
                    *value = left - right;
                    return 1;
                case AST_BINARY_MULTIPLY:
                    *value = left * right;
                    return 1;
                case AST_BINARY_DIVIDE:
                    if (
                        right == 0 ||
                        (
                            (int64_t)left == INT64_MIN &&
                            (int64_t)right == -1
                        )
                    ) {
                        return 0;
                    }
                    *value = (uint64_t)((int64_t)left / (int64_t)right);
                    return 1;
                case AST_BINARY_MODULO:
                    if (
                        right == 0 ||
                        (
                            (int64_t)left == INT64_MIN &&
                            (int64_t)right == -1
                        )
                    ) {
                        return 0;
                    }
                    *value = (uint64_t)((int64_t)left % (int64_t)right);
                    return 1;
                case AST_BINARY_EQUAL:
                    *value = left == right;
                    return 1;
                case AST_BINARY_NOT_EQUAL:
                    *value = left != right;
                    return 1;
                case AST_BINARY_LESS:
                    *value = (int64_t)left < (int64_t)right;
                    return 1;
                case AST_BINARY_LESS_EQUAL:
                    *value = (int64_t)left <= (int64_t)right;
                    return 1;
                case AST_BINARY_GREATER:
                    *value = (int64_t)left > (int64_t)right;
                    return 1;
                case AST_BINARY_GREATER_EQUAL:
                    *value = (int64_t)left >= (int64_t)right;
                    return 1;
                case AST_BINARY_AND:
                    *value = right != 0;
                    return 1;
                case AST_BINARY_OR:
                    *value = right != 0;
                    return 1;
            }
            return 0;

        default:
            return 0;
    }
}

static int
ir_append_constant(
    ir_module *module,
    const ast *expression,
    uint64_t value
)
{
    ir_constant *constants = realloc(
        module->constants,
        sizeof(*constants) * (size_t)(module->constant_count + 1)
    );
    if (!constants) {
        return 1;
    }

    module->constants = constants;
    module->constants[module->constant_count++] = (ir_constant) {
        .expression = expression,
        .value = value
    };
    return 0;
}

static int
ir_collect_constants(
    ir_module *module,
    ast *node
)
{
    uint64_t value;

    if (!node) {
        return 0;
    }

    switch (node->type) {
        case AST_BLOCK:
            for (unsigned long index = 0;
                 index < node->block.statements.count;
                 index++)
            {
                if (ir_collect_constants(
                        module,
                        node->block.statements.values[index]
                    )) {
                    return 1;
                }
            }
            break;
        case AST_VARIABLE_DECLARATION:
            if (ir_collect_constants(module, node->variable.value)) {
                return 1;
            }
            break;
        case AST_EXPRESSION_STATEMENT:
            if (ir_collect_constants(
                    module,
                    node->expression_statement.expression
                )) {
                return 1;
            }
            break;
        case AST_IF:
            if (
                ir_collect_constants(module, node->if_statement.condition) ||
                ir_collect_constants(module, node->if_statement.then_body) ||
                ir_collect_constants(module, node->if_statement.else_body)
            ) {
                return 1;
            }
            break;
        case AST_FOR:
            if (
                ir_collect_constants(module, node->for_statement.init) ||
                ir_collect_constants(module, node->for_statement.condition) ||
                ir_collect_constants(module, node->for_statement.increment) ||
                ir_collect_constants(module, node->for_statement.body)
            ) {
                return 1;
            }
            break;
        case AST_RETURN:
            for (unsigned long index = 0;
                 index < node->return_statement.values.count;
                 index++)
            {
                if (ir_collect_constants(
                        module,
                        node->return_statement.values.values[index]
                    )) {
                    return 1;
                }
            }
            break;
        case AST_ASSIGNMENT:
            if (
                ir_collect_constants(module, node->assignment.left) ||
                ir_collect_constants(module, node->assignment.right)
            ) {
                return 1;
            }
            break;
        case AST_MULTI_ASSIGNMENT:
            for (unsigned long index = 0;
                 index < node->multi_assignment.targets.count;
                 index++)
            {
                if (ir_collect_constants(
                        module,
                        node->multi_assignment.targets.values[index]
                    )) {
                    return 1;
                }
            }
            if (ir_collect_constants(module, node->multi_assignment.value)) {
                return 1;
            }
            break;
        case AST_ASM:
            for (unsigned long index = 0;
                 index < node->assembly.outputs.count;
                 index++)
            {
                if (ir_collect_constants(
                        module,
                        node->assembly.outputs.values[index]
                    )) {
                    return 1;
                }
            }
            for (unsigned long index = 0;
                 index < node->assembly.inputs.count;
                 index++)
            {
                if (ir_collect_constants(
                        module,
                        node->assembly.inputs.values[index]
                    )) {
                    return 1;
                }
            }
            break;
        case AST_UNARY:
            if (ir_collect_constants(module, node->unary.operand)) {
                return 1;
            }
            break;
        case AST_BINARY:
            if (
                ir_collect_constants(module, node->binary.left) ||
                ir_collect_constants(module, node->binary.right)
            ) {
                return 1;
            }
            break;
        case AST_CALL:
            for (unsigned long index = 0;
                 index < node->call.arguments.count;
                 index++)
            {
                if (ir_collect_constants(
                        module,
                        node->call.arguments.values[index]
                    )) {
                    return 1;
                }
            }
            break;
        case AST_INDEX:
            if (
                ir_collect_constants(module, node->index.object) ||
                ir_collect_constants(module, node->index.index)
            ) {
                return 1;
            }
            break;
        case AST_MEMBER:
            if (ir_collect_constants(module, node->member.object)) {
                return 1;
            }
            break;
        case AST_POINTER_MEMBER:
            if (ir_collect_constants(module, node->pointer_member.object)) {
                return 1;
            }
            break;
        case AST_ASM_OPERAND:
            if (ir_collect_constants(
                    module,
                    node->assembly_operand.expression
                )) {
                return 1;
            }
            break;
        default:
            break;
    }

    if (
        node->type != AST_INTEGER &&
        ir_constant_expression(node, &value) &&
        ir_append_constant(module, node, value)
    ) {
        return 1;
    }

    return 0;
}

static int
ir_mark_type(
    ir_context *context,
    const char *type_name
)
{
    ast *structure = ir_find_declaration(
        &context->program->program.structs,
        type_name
    );

    if (!structure) {
        return 0;
    }

    return ir_mark_declaration(context, structure);
}

static int
ir_contains_local(
    const ast *node,
    const char *name
)
{
    if (!node || !name) {
        return 0;
    }

    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            if (strcmp(node->variable.name, name) == 0) {
                return 1;
            }
            return ir_contains_local(node->variable.value, name);

        case AST_BLOCK:
            for (unsigned long index = 0;
                 index < node->block.statements.count;
                 index++)
            {
                if (ir_contains_local(
                        node->block.statements.values[index],
                        name
                    )) {
                    return 1;
                }
            }
            return 0;

        case AST_IF:
            return
                ir_contains_local(node->if_statement.then_body, name) ||
                ir_contains_local(node->if_statement.else_body, name);

        case AST_FOR:
            return
                ir_contains_local(node->for_statement.init, name) ||
                ir_contains_local(node->for_statement.body, name) ||
                ir_contains_local(node->for_statement.increment, name);

        default:
            return 0;
    }
}

static int
ir_function_has_local(
    const ast *function,
    const char *name
)
{
    for (unsigned long index = 0;
         index < function->function.parameters.count;
         index++)
    {
        ast *parameter = function->function.parameters.values[index];
        if (strcmp(parameter->parameter.name, name) == 0) {
            return 1;
        }
    }

    return ir_contains_local(function->function.body, name);
}

static int
ir_scan_node(
    ir_context *context,
    ast *node,
    ast *current_function
)
{
    if (!node) {
        return 0;
    }

    switch (node->type) {
        case AST_BLOCK:
            for (unsigned long index = 0;
                 index < node->block.statements.count;
                 index++)
            {
                if (ir_scan_node(
                        context,
                        node->block.statements.values[index],
                        current_function
                    )) {
                    return 1;
                }
            }
            return 0;

        case AST_VARIABLE_DECLARATION:
            if (ir_mark_type(context, node->variable.type)) {
                return 1;
            }
            return ir_scan_node(
                context,
                node->variable.value,
                current_function
            );

        case AST_EXPRESSION_STATEMENT:
            return ir_scan_node(
                context,
                node->expression_statement.expression,
                current_function
            );

        case AST_IF:
            return
                ir_scan_node(
                    context,
                    node->if_statement.condition,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->if_statement.then_body,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->if_statement.else_body,
                    current_function
                );

        case AST_FOR:
            return
                ir_scan_node(
                    context,
                    node->for_statement.init,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->for_statement.condition,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->for_statement.increment,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->for_statement.body,
                    current_function
                );

        case AST_RETURN:
            for (unsigned long index = 0;
                 index < node->return_statement.values.count;
                 index++)
            {
                if (ir_scan_node(
                        context,
                        node->return_statement.values.values[index],
                        current_function
                    )) {
                    return 1;
                }
            }
            return 0;

        case AST_ASSIGNMENT:
            return
                ir_scan_node(
                    context,
                    node->assignment.left,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->assignment.right,
                    current_function
                );

        case AST_MULTI_ASSIGNMENT:
            for (unsigned long index = 0;
                 index < node->multi_assignment.targets.count;
                 index++)
            {
                if (ir_scan_node(
                        context,
                        node->multi_assignment.targets.values[index],
                        current_function
                    )) {
                    return 1;
                }
            }
            return ir_scan_node(
                context,
                node->multi_assignment.value,
                current_function
            );

        case AST_ASM:
            for (unsigned long index = 0;
                 index < node->assembly.outputs.count;
                 index++)
            {
                if (ir_scan_node(
                        context,
                        node->assembly.outputs.values[index],
                        current_function
                    )) {
                    return 1;
                }
            }
            for (unsigned long index = 0;
                 index < node->assembly.inputs.count;
                 index++)
            {
                if (ir_scan_node(
                        context,
                        node->assembly.inputs.values[index],
                        current_function
                    )) {
                    return 1;
                }
            }
            return 0;

        case AST_IDENTIFIER:
        {
            ast *global;
            if (
                current_function != NULL &&
                ir_function_has_local(
                    current_function,
                    node->identifier.name
                )
            ) {
                return 0;
            }

            global = ir_find_declaration(
                &context->program->program.globals,
                node->identifier.name
            );
            return global
                ? ir_mark_declaration(context, global)
                : 0;
        }

        case AST_UNARY:
            return ir_scan_node(
                context,
                node->unary.operand,
                current_function
            );

        case AST_BINARY:
            return
                ir_scan_node(
                    context,
                    node->binary.left,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->binary.right,
                    current_function
                );

        case AST_CALL:
            if (
                node->call.function != NULL &&
                node->call.function->type == AST_IDENTIFIER
            ) {
                ast *function = ir_find_declaration(
                    &context->program->program.functions,
                    node->call.function->identifier.name
                );
                if (
                    function != NULL &&
                    ir_mark_declaration(context, function)
                ) {
                    return 1;
                }
            }
            for (unsigned long index = 0;
                 index < node->call.arguments.count;
                 index++)
            {
                if (ir_scan_node(
                        context,
                        node->call.arguments.values[index],
                        current_function
                    )) {
                    return 1;
                }
            }
            return 0;

        case AST_INDEX:
            return
                ir_scan_node(
                    context,
                    node->index.object,
                    current_function
                ) ||
                ir_scan_node(
                    context,
                    node->index.index,
                    current_function
                );

        case AST_MEMBER:
            return ir_scan_node(
                context,
                node->member.object,
                current_function
            );

        case AST_POINTER_MEMBER:
            return ir_scan_node(
                context,
                node->pointer_member.object,
                current_function
            );

        case AST_ASM_OPERAND:
            return ir_scan_node(
                context,
                node->assembly_operand.expression,
                current_function
            );

        default:
            return 0;
    }
}

static int
ir_mark_declaration(
    ir_context *context,
    ast *declaration
)
{
    ast_list *destination;
    int is_new;

    if (declaration->type == AST_FUNCTION) {
        destination = &context->module->functions;
    } else if (
        declaration->type == AST_GLOBAL ||
        declaration->type == AST_VARIABLE_DECLARATION
    ) {
        destination = &context->module->globals;
    } else if (declaration->type == AST_STRUCT) {
        destination = &context->module->structs;
    } else {
        return 0;
    }

    is_new = !ir_list_contains(destination, declaration);
    if (!is_new) {
        return 0;
    }

    if (ir_list_append(destination, declaration)) {
        return 1;
    }

    if (declaration->type == AST_FUNCTION) {
        for (unsigned long index = 0;
             index < declaration->function.return_types.count;
             index++)
        {
            ast *return_type =
                declaration->function.return_types.values[index];
            if (ir_mark_type(context, return_type->return_type.type)) {
                return 1;
            }
        }

        for (unsigned long index = 0;
             index < declaration->function.parameters.count;
             index++)
        {
            ast *parameter =
                declaration->function.parameters.values[index];
            if (ir_mark_type(context, parameter->parameter.type)) {
                return 1;
            }
        }

        return ir_scan_node(
            context,
            declaration->function.body,
            declaration
        );
    }

    if (
        declaration->type == AST_GLOBAL ||
        declaration->type == AST_VARIABLE_DECLARATION
    ) {
        if (ir_mark_type(context, declaration->variable.type)) {
            return 1;
        }
        return ir_scan_node(context, declaration->variable.value, NULL);
    }

    for (unsigned long index = 0;
         index < declaration->structure.fields.count;
         index++)
    {
        ast *field = declaration->structure.fields.values[index];
        if (ir_mark_type(context, field->struct_field.type)) {
            return 1;
        }
    }

    return 0;
}

static char *
ir_copy_text(
    const char *text
)
{
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static int
ir_append_block(
    ir_function *function,
    unsigned long *block_index
)
{
    ir_basic_block *blocks = realloc(
        function->blocks,
        sizeof(*blocks) * (size_t)(function->block_count + 1)
    );
    if (!blocks) {
        return 1;
    }

    function->blocks = blocks;
    *block_index = function->block_count++;
    function->blocks[*block_index] = (ir_basic_block) {0};
    return 0;
}

static ast *
ir_find_local_declaration(
    const ast *node,
    const char *name
)
{
    if (!node || !name) {
        return NULL;
    }

    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            return strcmp(node->variable.name, name) == 0
                ? (ast *)node
                : NULL;

        case AST_BLOCK:
            for (unsigned long index = 0;
                 index < node->block.statements.count;
                 index++)
            {
                ast *found = ir_find_local_declaration(
                    node->block.statements.values[index],
                    name
                );
                if (found) {
                    return found;
                }
            }
            return NULL;

        case AST_IF:
        {
            ast *found = ir_find_local_declaration(
                node->if_statement.then_body,
                name
            );
            return found ? found : ir_find_local_declaration(
                node->if_statement.else_body,
                name
            );
        }

        case AST_FOR:
        {
            ast *found = ir_find_local_declaration(
                node->for_statement.init,
                name
            );
            if (!found) {
                found = ir_find_local_declaration(
                    node->for_statement.body,
                    name
                );
            }
            if (!found) {
                found = ir_find_local_declaration(
                    node->for_statement.increment,
                    name
                );
            }
            return found;
        }

        default:
            return NULL;
    }
}

static ir_type
ir_type_from_declaration(
    const char *name,
    unsigned long pointer_depth,
    const ast *array_length
)
{
    return (ir_type) {
        .name = name,
        .pointer_depth = pointer_depth,
        .is_array = array_length != NULL,
        .array_length = array_length
    };
}

static ast *
ir_find_struct_field(
    const ast *program,
    const char *struct_name,
    const char *field_name
)
{
    ast *structure = ir_find_declaration(
        &program->program.structs,
        struct_name
    );
    if (!structure) {
        return NULL;
    }

    for (unsigned long index = 0;
         index < structure->structure.fields.count;
         index++)
    {
        ast *field = structure->structure.fields.values[index];
        if (strcmp(field->struct_field.name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
}

static int
ir_infer_expression_type(
    const ir_context *context,
    const ir_function *function,
    const ast *expression,
    ir_type *type,
    int *is_tuple
)
{
    ir_type left;
    ir_type right;
    ast *declaration;

    if (!expression || !type || !is_tuple) {
        return 1;
    }

    *is_tuple = 0;
    *type = (ir_type) {0};

    switch (expression->type) {
        case AST_INTEGER:
            type->name = "s8";
            return 0;

        case AST_STRING:
            type->name = "u1";
            type->pointer_depth = 1;
            return 0;

        case AST_IDENTIFIER:
            for (unsigned long index = 0;
                 index < function->declaration->function.parameters.count;
                 index++)
            {
                ast *parameter =
                    function->declaration->function.parameters.values[index];
                if (strcmp(
                        parameter->parameter.name,
                        expression->identifier.name
                    ) == 0) {
                    *type = ir_type_from_declaration(
                        parameter->parameter.type,
                        parameter->parameter.pointer_depth,
                        NULL
                    );
                    return 0;
                }
            }

            declaration = ir_find_local_declaration(
                function->declaration->function.body,
                expression->identifier.name
            );
            if (declaration) {
                *type = ir_type_from_declaration(
                    declaration->variable.type,
                    declaration->variable.pointer_depth,
                    declaration->variable.array_length
                );
                return 0;
            }

            declaration = ir_find_declaration(
                &context->program->program.globals,
                expression->identifier.name
            );
            if (declaration) {
                *type = ir_type_from_declaration(
                    declaration->variable.type,
                    declaration->variable.pointer_depth,
                    declaration->variable.array_length
                );
                return 0;
            }
            return 1;

        case AST_UNARY:
            if (ir_infer_expression_type(
                    context,
                    function,
                    expression->unary.operand,
                    &left,
                    is_tuple
                )) {
                return 1;
            }
            *type = left;
            switch (expression->unary.operator) {
                case AST_UNARY_NOT:
                    type->name = "u1";
                    type->pointer_depth = 0;
                    type->is_array = 0;
                    type->array_length = NULL;
                    return 0;
                case AST_UNARY_ADDRESS:
                    type->pointer_depth++;
                    type->is_array = 0;
                    type->array_length = NULL;
                    return 0;
                case AST_UNARY_DEREFERENCE:
                    if (type->pointer_depth == 0) {
                        return 1;
                    }
                    type->pointer_depth--;
                    type->is_array = 0;
                    type->array_length = NULL;
                    return 0;
                case AST_UNARY_PLUS:
                case AST_UNARY_MINUS:
                    return 0;
            }
            return 1;

        case AST_BINARY:
            if (
                ir_infer_expression_type(
                    context,
                    function,
                    expression->binary.left,
                    &left,
                    is_tuple
                ) ||
                ir_infer_expression_type(
                    context,
                    function,
                    expression->binary.right,
                    &right,
                    is_tuple
                )
            ) {
                return 1;
            }
            switch (expression->binary.operator) {
                case AST_BINARY_EQUAL:
                case AST_BINARY_NOT_EQUAL:
                case AST_BINARY_LESS:
                case AST_BINARY_LESS_EQUAL:
                case AST_BINARY_GREATER:
                case AST_BINARY_GREATER_EQUAL:
                case AST_BINARY_AND:
                case AST_BINARY_OR:
                    *type = (ir_type) { .name = "u1" };
                    return 0;
                case AST_BINARY_ADD:
                    if (left.pointer_depth != 0 || left.is_array) {
                        *type = left;
                    } else if (
                        right.pointer_depth != 0 ||
                        right.is_array
                    ) {
                        *type = right;
                    } else {
                        *type = (ir_type) { .name = "s8" };
                    }
                    return 0;
                case AST_BINARY_SUBTRACT:
                    if (
                        (left.pointer_depth != 0 || left.is_array) &&
                        right.pointer_depth == 0 &&
                        !right.is_array
                    ) {
                        *type = left;
                    } else {
                        *type = (ir_type) { .name = "s8" };
                    }
                    return 0;
                default:
                    *type = (ir_type) { .name = "s8" };
                    return 0;
            }

        case AST_CALL:
            if (
                !expression->call.function ||
                expression->call.function->type != AST_IDENTIFIER
            ) {
                return 1;
            }
            declaration = ir_find_declaration(
                &context->program->program.functions,
                expression->call.function->identifier.name
            );
            if (
                !declaration ||
                declaration->function.return_types.count == 0
            ) {
                return 1;
            }
            if (declaration->function.return_types.count > 1) {
                *is_tuple = 1;
                return 0;
            }
            {
                ast *return_type =
                    declaration->function.return_types.values[0];
                *type = ir_type_from_declaration(
                    return_type->return_type.type,
                    return_type->return_type.pointer_depth,
                    NULL
                );
            }
            return 0;

        case AST_INDEX:
            if (ir_infer_expression_type(
                    context,
                    function,
                    expression->index.object,
                    &left,
                    is_tuple
                )) {
                return 1;
            }
            if (left.is_array) {
                left.is_array = 0;
                left.array_length = NULL;
            } else if (left.pointer_depth != 0) {
                left.pointer_depth--;
            } else {
                return 1;
            }
            *type = left;
            return 0;

        case AST_MEMBER:
        case AST_POINTER_MEMBER:
        {
            const char *struct_name;
            if (ir_infer_expression_type(
                    context,
                    function,
                    expression->type == AST_MEMBER
                        ? expression->member.object
                        : expression->pointer_member.object,
                    &left,
                    is_tuple
                )) {
                return 1;
            }
            if (expression->type == AST_POINTER_MEMBER) {
                if (left.pointer_depth == 0) {
                    return 1;
                }
                left.pointer_depth--;
            }
            if (left.pointer_depth != 0 || left.is_array) {
                return 1;
            }
            struct_name = left.name;
            ast *field = ir_find_struct_field(
                context->program,
                struct_name,
                expression->type == AST_MEMBER
                    ? expression->member.name
                    : expression->pointer_member.name
            );
            if (!field) {
                return 1;
            }
            *type = ir_type_from_declaration(
                field->struct_field.type,
                field->struct_field.pointer_depth,
                field->struct_field.array_length
            );
            return 0;
        }

        default:
            return 1;
    }
}

static int
ir_lower_expression(
    const ir_context *context,
    ir_function *function,
    const ast *expression,
    int is_lvalue,
    unsigned long *value_id
)
{
    unsigned long left = ULONG_MAX;
    unsigned long right = ULONG_MAX;
    unsigned long *arguments = NULL;
    unsigned long argument_count = 0;
    ir_value_opcode opcode;
    ir_value *values;
    ir_type type;
    int is_tuple;

    if (!expression || !value_id) {
        return 1;
    }

    switch (expression->type) {
        case AST_INTEGER:
            opcode = IR_VALUE_INTEGER;
            break;
        case AST_STRING:
            opcode = IR_VALUE_STRING;
            break;
        case AST_IDENTIFIER:
            opcode = IR_VALUE_LOAD;
            break;
        case AST_UNARY:
            opcode = IR_VALUE_UNARY;
            if (ir_lower_expression(
                    context,
                    function,
                    expression->unary.operand,
                    expression->unary.operator == AST_UNARY_ADDRESS ||
                        expression->unary.operator == AST_UNARY_DEREFERENCE,
                    &left
                )) {
                return 1;
            }
            break;
        case AST_BINARY:
            opcode = IR_VALUE_BINARY;
            if (
                ir_lower_expression(
                    context,
                    function,
                    expression->binary.left,
                    0,
                    &left
                ) ||
                ir_lower_expression(
                    context,
                    function,
                    expression->binary.right,
                    0,
                    &right
                )
            ) {
                return 1;
            }
            break;
        case AST_CALL:
            opcode = IR_VALUE_CALL;
            argument_count = expression->call.arguments.count;
            arguments = calloc(
                (size_t)argument_count,
                sizeof(*arguments)
            );
            if (!arguments && argument_count != 0) {
                return 1;
            }
            for (unsigned long index = 0;
                 index < argument_count;
                 index++)
            {
                if (ir_lower_expression(
                        context,
                        function,
                        expression->call.arguments.values[index],
                        0,
                        &arguments[index]
                    )) {
                    free(arguments);
                    return 1;
                }
            }
            break;
        case AST_INDEX:
            opcode = IR_VALUE_INDEX;
            if (
                ir_lower_expression(
                    context,
                    function,
                    expression->index.object,
                    1,
                    &left
                ) ||
                ir_lower_expression(
                    context,
                    function,
                    expression->index.index,
                    0,
                    &right
                )
            ) {
                return 1;
            }
            break;
        case AST_MEMBER:
            opcode = IR_VALUE_MEMBER;
            if (ir_lower_expression(
                    context,
                    function,
                    expression->member.object,
                    1,
                    &left
                )) {
                return 1;
            }
            break;
        case AST_POINTER_MEMBER:
            opcode = IR_VALUE_POINTER_MEMBER;
            if (ir_lower_expression(
                    context,
                    function,
                    expression->pointer_member.object,
                    0,
                    &left
                )) {
                return 1;
            }
            break;
        default:
            return 1;
    }

    if (ir_infer_expression_type(
            context,
            function,
            expression,
            &type,
            &is_tuple
        )) {
        free(arguments);
        return 1;
    }

    values = realloc(
        function->values,
        sizeof(*values) * (size_t)(function->value_count + 1)
    );
    if (!values) {
        free(arguments);
        return 1;
    }

    function->values = values;
    *value_id = function->value_count;
    function->values[function->value_count++] = (ir_value) {
        .id = *value_id,
        .opcode = opcode,
        .type = type,
        .source = expression,
        .name =
            expression->type == AST_IDENTIFIER
                ? expression->identifier.name
                : expression->type == AST_CALL
                    ? expression->call.function &&
                        expression->call.function->type == AST_IDENTIFIER
                        ? expression->call.function->identifier.name
                        : NULL
                    : expression->type == AST_MEMBER
                        ? expression->member.name
                        : expression->type == AST_POINTER_MEMBER
                            ? expression->pointer_member.name
                            : NULL,
        .literal =
            expression->type == AST_INTEGER
                ? expression->integer.value
                : expression->type == AST_STRING
                    ? expression->string.value
                    : NULL,
        .unary_operator =
            expression->type == AST_UNARY
                ? expression->unary.operator
                : AST_UNARY_PLUS,
        .binary_operator =
            expression->type == AST_BINARY
                ? expression->binary.operator
                : AST_BINARY_ADD,
        .left = left,
        .right = right,
        .arguments = arguments,
        .argument_count = argument_count,
        .is_lvalue = is_lvalue,
        .is_tuple = is_tuple
    };
    return 0;
}

static int
ir_append_operand(
    ir_instruction *instruction,
    unsigned long value
)
{
    unsigned long *operands = realloc(
        instruction->operand_values,
        sizeof(*operands) * (size_t)(instruction->operand_count + 1)
    );
    if (!operands) {
        return 1;
    }

    instruction->operand_values = operands;
    instruction->operand_values[instruction->operand_count++] = value;
    return 0;
}

static void
ir_instruction_free(
    ir_instruction *instruction
)
{
    free(instruction->operand_values);
    free(instruction->output_constraints);
    free(instruction->input_constraints);
    free(instruction->clobbers);
    free(instruction->constant_values);
    instruction->operand_values = NULL;
    instruction->output_constraints = NULL;
    instruction->input_constraints = NULL;
    instruction->clobbers = NULL;
    instruction->constant_values = NULL;
}

static int
ir_append_instruction(
    const ir_context *context,
    ir_function *function,
    unsigned long block_index,
    const ast *source
)
{
    ir_instruction instruction = {
        .source = source,
        .file = source->file,
        .line = source->line,
        .column = source->column,
        .result_value = ULONG_MAX,
        .left_value = ULONG_MAX,
        .right_value = ULONG_MAX
    };
    const ast *expression = source;

    if (source->type == AST_EXPRESSION_STATEMENT) {
        expression = source->expression_statement.expression;
    }

    if (source->type == AST_VARIABLE_DECLARATION) {
        instruction.opcode = IR_INSTRUCTION_DECLARATION;
        instruction.symbol_name = source->variable.name;
        instruction.type = ir_type_from_declaration(
            source->variable.type,
            source->variable.pointer_depth,
            source->variable.array_length
        );
        if (
            source->variable.value &&
            ir_lower_expression(
                context,
                function,
                source->variable.value,
                0,
                &instruction.result_value
            )
        ) {
            return 1;
        }
    } else if (source->type == AST_ASSIGNMENT ||
               (expression && expression->type == AST_ASSIGNMENT)) {
        const ast *assignment =
            source->type == AST_ASSIGNMENT ? source : expression;

        instruction.opcode = IR_INSTRUCTION_ASSIGNMENT;
        if (
            ir_lower_expression(
                context,
                function,
                assignment->assignment.left,
                1,
                &instruction.left_value
            ) ||
            ir_lower_expression(
                context,
                function,
                assignment->assignment.right,
                0,
                &instruction.right_value
            )
        ) {
            return 1;
        }
        instruction.type =
            function->values[instruction.left_value].type;
    } else if (
        source->type == AST_MULTI_ASSIGNMENT ||
        (expression && expression->type == AST_MULTI_ASSIGNMENT)
    ) {
        const ast *assignment =
            source->type == AST_MULTI_ASSIGNMENT ? source : expression;

        instruction.opcode = IR_INSTRUCTION_MULTI_ASSIGNMENT;
        for (unsigned long index = 0;
             index < assignment->multi_assignment.targets.count;
             index++)
        {
            unsigned long target;
            if (
                ir_lower_expression(
                    context,
                    function,
                    assignment->multi_assignment.targets.values[index],
                    1,
                    &target
                ) ||
                ir_append_operand(&instruction, target)
            ) {
                free(instruction.operand_values);
                return 1;
            }
        }
        if (
            ir_lower_expression(
                context,
                function,
                assignment->multi_assignment.value,
                0,
                &instruction.result_value
            ) ||
            ir_append_operand(
                &instruction,
                instruction.result_value
            )
        ) {
            free(instruction.operand_values);
            return 1;
        }
    } else if (source->type == AST_RETURN) {
        instruction.opcode = IR_INSTRUCTION_RETURN;
        for (unsigned long index = 0;
             index < source->return_statement.values.count;
             index++)
        {
            unsigned long value;
            if (
                ir_lower_expression(
                    context,
                    function,
                    source->return_statement.values.values[index],
                    0,
                    &value
                ) ||
                ir_append_operand(&instruction, value)
            ) {
                free(instruction.operand_values);
                return 1;
            }
        }
    } else if (source->type == AST_ASM) {
        instruction.opcode = IR_INSTRUCTION_ASSEMBLY;
        instruction.assembly_template = source->assembly.template;
        instruction.output_count = source->assembly.outputs.count;
        instruction.input_count = source->assembly.inputs.count;
        instruction.clobber_count = source->assembly.clobbers.count;
        instruction.output_constraints = calloc(
            (size_t)instruction.output_count,
            sizeof(*instruction.output_constraints)
        );
        instruction.input_constraints = calloc(
            (size_t)instruction.input_count,
            sizeof(*instruction.input_constraints)
        );
        instruction.clobbers = calloc(
            (size_t)instruction.clobber_count,
            sizeof(*instruction.clobbers)
        );
        if (
            (!instruction.output_constraints &&
             instruction.output_count != 0) ||
            (!instruction.input_constraints &&
             instruction.input_count != 0) ||
            (!instruction.clobbers &&
             instruction.clobber_count != 0)
        ) {
            ir_instruction_free(&instruction);
            return 1;
        }

        for (unsigned long index = 0;
             index < source->assembly.outputs.count;
             index++)
        {
            ast *operand = source->assembly.outputs.values[index];
            unsigned long value;
            instruction.output_constraints[index] =
                operand->assembly_operand.constraint;
            if (
                ir_lower_expression(
                    context,
                    function,
                    operand->assembly_operand.expression,
                    1,
                    &value
                ) ||
                ir_append_operand(&instruction, value)
            ) {
                ir_instruction_free(&instruction);
                return 1;
            }
        }
        for (unsigned long index = 0;
             index < source->assembly.inputs.count;
             index++)
        {
            ast *operand = source->assembly.inputs.values[index];
            unsigned long value;
            instruction.input_constraints[index] =
                operand->assembly_operand.constraint;
            if (
                ir_lower_expression(
                    context,
                    function,
                    operand->assembly_operand.expression,
                    0,
                    &value
                ) ||
                ir_append_operand(&instruction, value)
            ) {
                ir_instruction_free(&instruction);
                return 1;
            }
        }
        for (unsigned long index = 0;
             index < source->assembly.clobbers.count;
             index++)
        {
            ast *clobber = source->assembly.clobbers.values[index];
            instruction.clobbers[index] = clobber->string.value;
        }
    } else {
        instruction.opcode = IR_INSTRUCTION_EXPRESSION;
        if (
            expression &&
            ir_lower_expression(
                context,
                function,
                expression,
                0,
                &instruction.result_value
            )
        ) {
            return 1;
        }
    }

    ir_basic_block *block = &function->blocks[block_index];
    ir_instruction *instructions = realloc(
        block->instructions,
        sizeof(*instructions) * (size_t)(block->instruction_count + 1)
    );
    if (!instructions) {
        ir_instruction_free(&instruction);
        return 1;
    }

    block->instructions = instructions;
    block->instructions[block->instruction_count++] = instruction;
    return 0;
}

static int
ir_set_jump(
    ir_function *function,
    unsigned long block_index,
    unsigned long target
)
{
    ir_terminator *terminator =
        &function->blocks[block_index].terminator;
    if (terminator->kind != IR_TERMINATOR_NONE) {
        return 1;
    }

    *terminator = (ir_terminator) {
        .kind = IR_TERMINATOR_JUMP,
        .true_block = target
    };
    return 0;
}

static int
ir_lower_statement(
    const ir_context *context,
    ir_function *function,
    const ast *statement,
    unsigned long *current_block
)
{
    if (!statement) {
        return 0;
    }

    switch (statement->type) {
        case AST_BLOCK:
            for (unsigned long index = 0;
                 index < statement->block.statements.count;
                 index++)
            {
                if (ir_lower_statement(
                        context,
                        function,
                        statement->block.statements.values[index],
                        current_block
                    )) {
                    return 1;
                }
            }
            return 0;

        case AST_IF:
        {
            unsigned long then_block;
            unsigned long else_block;
            unsigned long merge_block;
            unsigned long then_end;
            unsigned long else_end;
            unsigned long condition_value;
            int has_else = statement->if_statement.else_body != NULL;

            if (
                ir_append_block(function, &then_block) ||
                (has_else && ir_append_block(function, &else_block)) ||
                ir_append_block(function, &merge_block)
            ) {
                return 1;
            }
            if (!has_else) {
                else_block = merge_block;
            }

            if (ir_lower_expression(
                    context,
                    function,
                    statement->if_statement.condition,
                    0,
                    &condition_value
                )) {
                return 1;
            }

            function->blocks[*current_block].terminator =
                (ir_terminator) {
                    .kind = IR_TERMINATOR_BRANCH,
                    .condition_value = condition_value,
                    .true_block = then_block,
                    .false_block = else_block
                };

            then_end = then_block;
            if (ir_lower_statement(
                    context,
                    function,
                    statement->if_statement.then_body,
                    &then_end
                ) ||
                ir_set_jump(function, then_end, merge_block)
            ) {
                return 1;
            }

            if (has_else) {
                else_end = else_block;
                if (ir_lower_statement(
                        context,
                        function,
                        statement->if_statement.else_body,
                        &else_end
                    ) ||
                    ir_set_jump(function, else_end, merge_block)
                ) {
                    return 1;
                }
            }

            *current_block = merge_block;
            return 0;
        }

        case AST_FOR:
        {
            unsigned long condition_block;
            unsigned long body_block;
            unsigned long increment_block;
            unsigned long exit_block;
            unsigned long body_end;
            unsigned long increment_end;
            unsigned long condition_value = ULONG_MAX;

            if (
                ir_lower_statement(
                    context,
                    function,
                    statement->for_statement.init,
                    current_block
                ) ||
                ir_append_block(function, &condition_block) ||
                ir_append_block(function, &body_block) ||
                ir_append_block(function, &increment_block) ||
                ir_append_block(function, &exit_block) ||
                ir_set_jump(function, *current_block, condition_block)
            ) {
                return 1;
            }

            if (statement->for_statement.condition) {
                if (ir_lower_expression(
                        context,
                        function,
                        statement->for_statement.condition,
                        0,
                        &condition_value
                    )) {
                    return 1;
                }
                function->blocks[condition_block].terminator =
                    (ir_terminator) {
                        .kind = IR_TERMINATOR_BRANCH,
                        .condition_value = condition_value,
                        .true_block = body_block,
                        .false_block = exit_block
                    };
            } else if (ir_set_jump(function, condition_block, body_block)) {
                return 1;
            }

            body_end = body_block;
            if (
                ir_lower_statement(
                    context,
                    function,
                    statement->for_statement.body,
                    &body_end
                ) ||
                ir_set_jump(function, body_end, increment_block)
            ) {
                return 1;
            }

            increment_end = increment_block;
            if (
                ir_lower_statement(
                    context,
                    function,
                    statement->for_statement.increment,
                    &increment_end
                ) ||
                ir_set_jump(function, increment_end, condition_block)
            ) {
                return 1;
            }

            *current_block = exit_block;
            return 0;
        }

        default:
            return ir_append_instruction(
                context,
                function,
                *current_block,
                statement
            );
    }
}

static int
ir_validate_function(
    const ir_function *function
);

static int
ir_lower_functions(
    ir_module *module
)
{
    module->lowered_functions = calloc(
        (size_t)module->functions.count,
        sizeof(*module->lowered_functions)
    );
    if (!module->lowered_functions && module->functions.count != 0) {
        return 1;
    }

    module->lowered_function_count = module->functions.count;
    for (unsigned long index = 0;
         index < module->functions.count;
         index++)
    {
        ir_function *function = &module->lowered_functions[index];
        unsigned long entry_block;
        unsigned long current_block;

        function->declaration = module->functions.values[index];
        if (ir_append_block(function, &entry_block)) {
            return 1;
        }

        current_block = entry_block;
        if (ir_lower_statement(
                &(ir_context) {
                    .program = module->source_program,
                    .module = module
                },
                function,
                function->declaration->function.body,
                &current_block
            )) {
            return 1;
        }
        if (ir_validate_function(function)) {
            return 1;
        }
    }

    return 0;
}

static int
ir_valid_value_id(
    const ir_function *function,
    unsigned long value_id
)
{
    return value_id < function->value_count;
}

static int
ir_validate_function(
    const ir_function *function
)
{
    for (unsigned long value_index = 0;
         value_index < function->value_count;
         value_index++)
    {
        const ir_value *value = &function->values[value_index];
        if (value->id != value_index) {
            return 1;
        }
        if (
            value->is_tuple
                ? value->opcode != IR_VALUE_CALL
                : value->type.name == NULL
        ) {
            return 1;
        }
        if (
            (value->left != ULONG_MAX && value->left >= value->id) ||
            (value->right != ULONG_MAX && value->right >= value->id)
        ) {
            return 1;
        }
        for (unsigned long index = 0;
             index < value->argument_count;
             index++)
        {
            if (
                !ir_valid_value_id(function, value->arguments[index]) ||
                value->arguments[index] >= value->id
            ) {
                return 1;
            }
        }
        switch (value->opcode) {
            case IR_VALUE_INTEGER:
                if (value->literal == NULL) {
                    return 1;
                }
                if (
                    value->left != ULONG_MAX ||
                    value->right != ULONG_MAX ||
                    value->argument_count != 0
                ) {
                    return 1;
                }
                break;
            case IR_VALUE_STRING:
                if (value->literal == NULL) {
                    return 1;
                }
                if (
                    value->left != ULONG_MAX ||
                    value->right != ULONG_MAX ||
                    value->argument_count != 0
                ) {
                    return 1;
                }
                break;
            case IR_VALUE_LOAD:
                if (
                    value->opcode == IR_VALUE_LOAD &&
                    value->name == NULL
                ) {
                    return 1;
                }
                if (
                    value->left != ULONG_MAX ||
                    value->right != ULONG_MAX ||
                    value->argument_count != 0
                ) {
                    return 1;
                }
                break;
            case IR_VALUE_UNARY:
            case IR_VALUE_MEMBER:
            case IR_VALUE_POINTER_MEMBER:
                if (
                    value->left == ULONG_MAX ||
                    value->right != ULONG_MAX ||
                    value->argument_count != 0 ||
                    (
                        value->opcode != IR_VALUE_UNARY &&
                        value->name == NULL
                    )
                ) {
                    return 1;
                }
                break;
            case IR_VALUE_BINARY:
            case IR_VALUE_INDEX:
                if (
                    value->left == ULONG_MAX ||
                    value->right == ULONG_MAX ||
                    value->argument_count != 0
                ) {
                    return 1;
                }
                break;
            case IR_VALUE_CALL:
                if (
                    value->left != ULONG_MAX ||
                    value->right != ULONG_MAX ||
                    value->name == NULL
                ) {
                    return 1;
                }
                break;
        }
    }

    for (unsigned long block_index = 0;
         block_index < function->block_count;
         block_index++)
    {
        const ir_basic_block *block = &function->blocks[block_index];
        for (unsigned long instruction_index = 0;
             instruction_index < block->instruction_count;
             instruction_index++)
        {
            const ir_instruction *instruction =
                &block->instructions[instruction_index];

            if (
                (instruction->result_value != ULONG_MAX &&
                 !ir_valid_value_id(function, instruction->result_value)) ||
                (instruction->left_value != ULONG_MAX &&
                 !ir_valid_value_id(function, instruction->left_value)) ||
                (instruction->right_value != ULONG_MAX &&
                 !ir_valid_value_id(function, instruction->right_value))
            ) {
                return 1;
            }
            for (unsigned long index = 0;
                 index < instruction->operand_count;
                 index++)
            {
                if (!ir_valid_value_id(
                        function,
                        instruction->operand_values[index]
                    )) {
                    return 1;
                }
            }

            if (
                instruction->opcode == IR_INSTRUCTION_DECLARATION &&
                instruction->type.name == NULL
            ) {
                return 1;
            }
            if (
                instruction->constant_value_count != 0 &&
                (
                    instruction->opcode !=
                        IR_INSTRUCTION_MULTI_ASSIGNMENT ||
                    instruction->constant_values == NULL ||
                    instruction->constant_value_count !=
                        instruction->operand_count - 1
                )
            ) {
                return 1;
            }
            switch (instruction->opcode) {
                case IR_INSTRUCTION_DECLARATION:
                    if (
                        instruction->result_value != ULONG_MAX &&
                        !ir_valid_value_id(
                            function,
                            instruction->result_value
                        )
                    ) {
                        return 1;
                    }
                    break;
                case IR_INSTRUCTION_EXPRESSION:
                    if (
                        instruction->result_value == ULONG_MAX &&
                        instruction->source->type != AST_EXPRESSION_STATEMENT
                    ) {
                        return 1;
                    }
                    break;
                case IR_INSTRUCTION_ASSIGNMENT:
                    if (
                        instruction->left_value == ULONG_MAX ||
                        instruction->right_value == ULONG_MAX ||
                        instruction->type.name == NULL
                    ) {
                        return 1;
                    }
                    break;
                case IR_INSTRUCTION_MULTI_ASSIGNMENT:
                    if (instruction->operand_count < 2) {
                        return 1;
                    }
                    break;
                case IR_INSTRUCTION_RETURN:
                case IR_INSTRUCTION_ASSEMBLY:
                    break;
            }
        }

        switch (block->terminator.kind) {
            case IR_TERMINATOR_NONE:
                break;
            case IR_TERMINATOR_JUMP:
                if (block->terminator.true_block >= function->block_count) {
                    return 1;
                }
                break;
            case IR_TERMINATOR_BRANCH:
                if (
                    !ir_valid_value_id(
                        function,
                        block->terminator.condition_value
                    ) ||
                    block->terminator.true_block >= function->block_count ||
                    block->terminator.false_block >= function->block_count
                ) {
                    return 1;
                }
                break;
        }
    }

    return 0;
}

static void
ir_fold_value(
    ir_function *function,
    ir_value *value
)
{
    uint64_t left;
    uint64_t right;
    char *end;
    long long integer;

    if (value->opcode == IR_VALUE_INTEGER) {
        errno = 0;
        integer = strtoll(value->literal, &end, 10);
        if (errno == 0 && *end == '\0') {
            value->is_constant = 1;
            value->constant_value = (uint64_t)integer;
        }
        return;
    }

    if (value->opcode == IR_VALUE_UNARY) {
        const ir_value *operand = &function->values[value->left];
        if (!operand->is_constant) {
            return;
        }
        switch (value->unary_operator) {
            case AST_UNARY_PLUS:
                value->constant_value = operand->constant_value;
                break;
            case AST_UNARY_MINUS:
                value->constant_value =
                    UINT64_C(0) - operand->constant_value;
                break;
            case AST_UNARY_NOT:
                value->constant_value = operand->constant_value == 0;
                break;
            default:
                return;
        }
        value->is_constant = 1;
        return;
    }

    if (value->opcode != IR_VALUE_BINARY) {
        return;
    }

    const ir_value *left_value = &function->values[value->left];
    if (!left_value->is_constant) {
        return;
    }
    left = left_value->constant_value;

    if (
        value->binary_operator == AST_BINARY_AND &&
        left == 0
    ) {
        value->is_constant = 1;
        value->constant_value = 0;
        return;
    }
    if (
        value->binary_operator == AST_BINARY_OR &&
        left != 0
    ) {
        value->is_constant = 1;
        value->constant_value = 1;
        return;
    }

    const ir_value *right_value = &function->values[value->right];
    if (!right_value->is_constant) {
        return;
    }
    right = right_value->constant_value;

    switch (value->binary_operator) {
        case AST_BINARY_ADD:
            value->constant_value = left + right;
            break;
        case AST_BINARY_SUBTRACT:
            value->constant_value = left - right;
            break;
        case AST_BINARY_MULTIPLY:
            value->constant_value = left * right;
            break;
        case AST_BINARY_DIVIDE:
            if (
                right == 0 ||
                (
                    (int64_t)left == INT64_MIN &&
                    (int64_t)right == -1
                )
            ) {
                return;
            }
            value->constant_value =
                (uint64_t)((int64_t)left / (int64_t)right);
            break;
        case AST_BINARY_MODULO:
            if (
                right == 0 ||
                (
                    (int64_t)left == INT64_MIN &&
                    (int64_t)right == -1
                )
            ) {
                return;
            }
            value->constant_value =
                (uint64_t)((int64_t)left % (int64_t)right);
            break;
        case AST_BINARY_EQUAL:
            value->constant_value = left == right;
            break;
        case AST_BINARY_NOT_EQUAL:
            value->constant_value = left != right;
            break;
        case AST_BINARY_LESS:
            value->constant_value =
                (int64_t)left < (int64_t)right;
            break;
        case AST_BINARY_LESS_EQUAL:
            value->constant_value =
                (int64_t)left <= (int64_t)right;
            break;
        case AST_BINARY_GREATER:
            value->constant_value =
                (int64_t)left > (int64_t)right;
            break;
        case AST_BINARY_GREATER_EQUAL:
            value->constant_value =
                (int64_t)left >= (int64_t)right;
            break;
        case AST_BINARY_AND:
            value->constant_value = right != 0;
            break;
        case AST_BINARY_OR:
            value->constant_value = right != 0;
            break;
    }
    value->is_constant = 1;
}

typedef struct ir_constant_binding
{
    const char *name;
    uint64_t value;
    int known;
} ir_constant_binding;

static unsigned long
ir_binding_find(
    const ir_constant_binding *bindings,
    unsigned long count,
    const char *name
)
{
    if (!name) {
        return ULONG_MAX;
    }

    for (unsigned long index = 0; index < count; index++) {
        if (strcmp(bindings[index].name, name) == 0) {
            return index;
        }
    }

    return ULONG_MAX;
}

static int
ir_binding_set(
    ir_constant_binding *bindings,
    unsigned long *count,
    unsigned long capacity,
    const char *name,
    int known,
    uint64_t value
)
{
    unsigned long index = ir_binding_find(bindings, *count, name);

    if (index == ULONG_MAX) {
        if (*count >= capacity || !name) {
            return 1;
        }
        index = (*count)++;
        bindings[index].name = name;
    }

    bindings[index].known = known;
    bindings[index].value = value;
    return 0;
}

static void
ir_bindings_invalidate(
    ir_constant_binding *bindings,
    unsigned long count
)
{
    for (unsigned long index = 0; index < count; index++) {
        bindings[index].known = 0;
    }
}

static int
ir_normalize_integer(
    const char *type,
    unsigned long pointer_depth,
    int is_array,
    uint64_t *value
)
{
    char *end;
    unsigned long bytes;
    unsigned long bits;
    uint64_t mask;

    if (
        !type ||
        !value ||
        pointer_depth != 0 ||
        is_array ||
        (type[0] != 'u' && type[0] != 's')
    ) {
        return 0;
    }

    bytes = strtoul(type + 1, &end, 10);
    if (
        end == type + 1 ||
        *end != '\0' ||
        (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8)
    ) {
        return 0;
    }

    bits = bytes * 8;
    mask = bits == 64
        ? UINT64_MAX
        : (UINT64_C(1) << bits) - 1;
    *value &= mask;
    if (
        type[0] == 's' &&
        bits < 64 &&
        (*value & (UINT64_C(1) << (bits - 1))) != 0
    ) {
        *value |= ~mask;
    }
    return 1;
}

static int
ir_evaluate_constant(
    const ir_function *function,
    unsigned long value_id,
    const ir_constant_binding *bindings,
    unsigned long binding_count,
    uint64_t *result
)
{
    const ir_value *value;
    uint64_t left;
    uint64_t right;

    if (
        !function ||
        value_id >= function->value_count ||
        !result
    ) {
        return 0;
    }

    value = &function->values[value_id];
    switch (value->opcode) {
        case IR_VALUE_INTEGER:
            return value->is_constant
                ? (*result = value->constant_value, 1)
                : 0;

        case IR_VALUE_LOAD:
        {
            unsigned long binding = ir_binding_find(
                bindings,
                binding_count,
                value->name
            );
            uint64_t normalized_type_check = 0;
            if (
                binding == ULONG_MAX ||
                !bindings[binding].known ||
                !ir_normalize_integer(
                    value->type.name,
                    value->type.pointer_depth,
                    value->type.is_array,
                    &normalized_type_check
                )
            ) {
                return 0;
            }
            *result = bindings[binding].value;
            return 1;
        }

        case IR_VALUE_UNARY:
            if (!ir_evaluate_constant(
                    function,
                    value->left,
                    bindings,
                    binding_count,
                    &left
                )) {
                return 0;
            }
            switch (value->unary_operator) {
                case AST_UNARY_PLUS:
                    *result = left;
                    return 1;
                case AST_UNARY_MINUS:
                    *result = UINT64_C(0) - left;
                    return 1;
                case AST_UNARY_NOT:
                    *result = left == 0;
                    return 1;
                default:
                    return 0;
            }

        case IR_VALUE_BINARY:
            if (!ir_evaluate_constant(
                    function,
                    value->left,
                    bindings,
                    binding_count,
                    &left
                )) {
                return 0;
            }
            if (
                value->binary_operator == AST_BINARY_AND &&
                left == 0
            ) {
                *result = 0;
                return 1;
            }
            if (
                value->binary_operator == AST_BINARY_OR &&
                left != 0
            ) {
                *result = 1;
                return 1;
            }
            if (!ir_evaluate_constant(
                    function,
                    value->right,
                    bindings,
                    binding_count,
                    &right
                )) {
                return 0;
            }

            switch (value->binary_operator) {
                case AST_BINARY_ADD: *result = left + right; break;
                case AST_BINARY_SUBTRACT: *result = left - right; break;
                case AST_BINARY_MULTIPLY: *result = left * right; break;
                case AST_BINARY_DIVIDE:
                    if (
                        right == 0 ||
                        (
                            (int64_t)left == INT64_MIN &&
                            (int64_t)right == -1
                        )
                    ) {
                        return 0;
                    }
                    *result =
                        (uint64_t)((int64_t)left / (int64_t)right);
                    break;
                case AST_BINARY_MODULO:
                    if (
                        right == 0 ||
                        (
                            (int64_t)left == INT64_MIN &&
                            (int64_t)right == -1
                        )
                    ) {
                        return 0;
                    }
                    *result =
                        (uint64_t)((int64_t)left % (int64_t)right);
                    break;
                case AST_BINARY_EQUAL: *result = left == right; break;
                case AST_BINARY_NOT_EQUAL: *result = left != right; break;
                case AST_BINARY_LESS:
                    *result = (int64_t)left < (int64_t)right;
                    break;
                case AST_BINARY_LESS_EQUAL:
                    *result = (int64_t)left <= (int64_t)right;
                    break;
                case AST_BINARY_GREATER:
                    *result = (int64_t)left > (int64_t)right;
                    break;
                case AST_BINARY_GREATER_EQUAL:
                    *result = (int64_t)left >= (int64_t)right;
                    break;
                case AST_BINARY_AND: *result = right != 0; break;
                case AST_BINARY_OR: *result = right != 0; break;
            }
            return 1;

        default:
            return 0;
    }
}

static void
ir_mark_value_constants(
    ir_function *function,
    unsigned long value_id,
    ir_constant_binding *bindings,
    unsigned long binding_count
)
{
    ir_value *value;

    if (!function || value_id >= function->value_count) {
        return;
    }

    value = &function->values[value_id];
    switch (value->opcode) {
        case IR_VALUE_INTEGER:
            ir_fold_value(function, value);
            return;

        case IR_VALUE_LOAD:
        {
            unsigned long binding = ir_binding_find(
                bindings,
                binding_count,
                value->name
            );
            uint64_t normalized_type_check = 0;
            value->is_constant =
                binding != ULONG_MAX &&
                bindings[binding].known &&
                ir_normalize_integer(
                    value->type.name,
                    value->type.pointer_depth,
                    value->type.is_array,
                    &normalized_type_check
                );
            if (value->is_constant) {
                value->constant_value = bindings[binding].value;
            }
            return;
        }

        case IR_VALUE_UNARY:
            if (value->unary_operator == AST_UNARY_ADDRESS) {
                const ir_value *operand =
                    &function->values[value->left];
                if (
                    operand->opcode == IR_VALUE_LOAD &&
                    ir_binding_find(
                        bindings,
                        binding_count,
                        operand->name
                    ) != ULONG_MAX
                ) {
                    unsigned long binding = ir_binding_find(
                        bindings,
                        binding_count,
                        operand->name
                    );
                    bindings[binding].known = 0;
                } else {
                    ir_bindings_invalidate(bindings, binding_count);
                }
                ir_mark_value_constants(
                    function,
                    value->left,
                    bindings,
                    binding_count
                );
                value->is_constant = 0;
                return;
            }
            ir_mark_value_constants(
                function,
                value->left,
                bindings,
                binding_count
            );
            value->is_constant = 0;
            ir_fold_value(function, value);
            return;

        case IR_VALUE_BINARY:
            ir_mark_value_constants(
                function,
                value->left,
                bindings,
                binding_count
            );
            ir_mark_value_constants(
                function,
                value->right,
                bindings,
                binding_count
            );
            value->is_constant = 0;
            ir_fold_value(function, value);
            return;

        case IR_VALUE_CALL:
            for (unsigned long index = 0;
                 index < value->argument_count;
                 index++)
            {
                ir_mark_value_constants(
                    function,
                    value->arguments[index],
                    bindings,
                    binding_count
                );
            }
            value->is_constant = 0;
            return;

        case IR_VALUE_INDEX:
        case IR_VALUE_MEMBER:
        case IR_VALUE_POINTER_MEMBER:
            ir_mark_value_constants(
                function,
                value->left,
                bindings,
                binding_count
            );
            if (value->right != ULONG_MAX) {
                ir_mark_value_constants(
                    function,
                    value->right,
                    bindings,
                    binding_count
                );
            }
            value->is_constant = 0;
            return;

        case IR_VALUE_STRING:
            value->is_constant = 0;
            return;
    }
}

static int
ir_call_may_alias_locals(
    const ir_function *function,
    const ir_value *call
)
{
    for (unsigned long index = 0;
         index < call->argument_count;
         index++)
    {
        const ir_value *argument =
            &function->values[call->arguments[index]];
        if (
            (argument->type.pointer_depth == 0 &&
             !argument->type.is_array) ||
            argument->opcode == IR_VALUE_STRING ||
            (
                argument->opcode == IR_VALUE_INTEGER &&
                argument->is_constant &&
                argument->constant_value == 0
            )
        ) {
            continue;
        }
        return 1;
    }

    return 0;
}

static const ir_function *
ir_find_lowered_function(
    const ir_module *module,
    const char *name
)
{
    for (unsigned long index = 0;
         index < module->lowered_function_count;
         index++)
    {
        const ir_function *function = &module->lowered_functions[index];
        if (
            function->declaration &&
            strcmp(function->declaration->function.name, name) == 0
        ) {
            return function;
        }
    }
    return NULL;
}

static int
ir_evaluate_pure_call(
    const ir_module *module,
    ir_function *caller,
    const ir_value *call,
    const ir_constant_binding *caller_bindings,
    unsigned long caller_binding_count,
    uint64_t **results,
    unsigned long *result_count
)
{
    const ir_function *callee;
    const ir_instruction *return_instruction = NULL;
    ir_constant_binding *parameter_bindings;
    unsigned long parameter_count;
    unsigned long parameter_binding_count = 0;
    unsigned long return_count;
    uint64_t *constant_results;

    if (
        !module ||
        !call ||
        call->opcode != IR_VALUE_CALL ||
        !results ||
        !result_count ||
        ir_call_may_alias_locals(caller, call)
    ) {
        return 0;
    }

    callee = ir_find_lowered_function(module, call->name);
    if (
        !callee ||
        !callee->declaration ||
        callee->declaration->function.is_foreign ||
        callee->block_count != 1 ||
        callee->blocks[0].instruction_count != 1 ||
        callee->blocks[0].terminator.kind != IR_TERMINATOR_NONE
    ) {
        return 0;
    }

    return_instruction = &callee->blocks[0].instructions[0];
    parameter_count = callee->declaration->function.parameters.count;
    return_count = return_instruction->operand_count;
    if (
        return_instruction->opcode != IR_INSTRUCTION_RETURN ||
        call->argument_count != parameter_count ||
        return_count !=
            callee->declaration->function.return_types.count
    ) {
        return 0;
    }

    parameter_bindings = calloc(
        (size_t)(parameter_count == 0 ? 1 : parameter_count),
        sizeof(*parameter_bindings)
    );
    constant_results = calloc(
        (size_t)return_count,
        sizeof(*constant_results)
    );
    if (!parameter_bindings || (!constant_results && return_count != 0)) {
        free(parameter_bindings);
        free(constant_results);
        return -1;
    }

    for (unsigned long index = 0; index < parameter_count; index++) {
        ast *parameter =
            callee->declaration->function.parameters.values[index];
        uint64_t argument_value;
        if (
            !ir_evaluate_constant(
                caller,
                call->arguments[index],
                caller_bindings,
                caller_binding_count,
                &argument_value
            ) ||
            !ir_normalize_integer(
                parameter->parameter.type,
                parameter->parameter.pointer_depth,
                0,
                &argument_value
            ) ||
            ir_binding_set(
                parameter_bindings,
                &parameter_binding_count,
                parameter_count,
                parameter->parameter.name,
                1,
                argument_value
            )
        ) {
            free(parameter_bindings);
            free(constant_results);
            return 0;
        }
    }

    for (unsigned long index = 0; index < return_count; index++) {
        ast *return_type =
            callee->declaration->function.return_types.values[index];
        if (
            !ir_normalize_integer(
                return_type->return_type.type,
                return_type->return_type.pointer_depth,
                0,
                &constant_results[index]
            ) ||
            !ir_evaluate_constant(
                callee,
                return_instruction->operand_values[index],
                parameter_bindings,
                parameter_binding_count,
                &constant_results[index]
            ) ||
            !ir_normalize_integer(
                return_type->return_type.type,
                return_type->return_type.pointer_depth,
                0,
                &constant_results[index]
            )
        ) {
            free(parameter_bindings);
            free(constant_results);
            return 0;
        }
    }

    free(parameter_bindings);
    *results = constant_results;
    *result_count = return_count;
    return 1;
}

static int
ir_optimize_straight_line(
    const ir_module *module,
    ir_function *function
)
{
    ir_basic_block *block;
    ir_constant_binding *bindings;
    unsigned long binding_capacity;
    unsigned long binding_count = 0;
    unsigned long declaration_count = 0;

    if (function->block_count != 1) {
        return 0;
    }

    block = &function->blocks[0];
    for (unsigned long index = 0;
         index < block->instruction_count;
         index++)
    {
        if (
            block->instructions[index].opcode ==
            IR_INSTRUCTION_DECLARATION
        ) {
            declaration_count++;
        }
    }
    binding_capacity =
        function->declaration->function.parameters.count +
        declaration_count + 1;
    bindings = calloc((size_t)binding_capacity, sizeof(*bindings));
    if (!bindings) {
        return 1;
    }

    for (unsigned long index = 0;
         index < function->declaration->function.parameters.count;
         index++)
    {
        ast *parameter =
            function->declaration->function.parameters.values[index];
        if (ir_binding_set(
                bindings,
                &binding_count,
                binding_capacity,
                parameter->parameter.name,
                0,
                0
            )) {
            free(bindings);
            return 1;
        }
    }

    for (unsigned long index = 0;
         index < block->instruction_count;
         index++)
    {
        ir_instruction *instruction = &block->instructions[index];
        uint64_t value;

        switch (instruction->opcode) {
            case IR_INSTRUCTION_DECLARATION:
                if (instruction->result_value != ULONG_MAX) {
                    ir_mark_value_constants(
                        function,
                        instruction->result_value,
                        bindings,
                        binding_count
                    );
                }
                value = 0;
                if (
                    instruction->result_value != ULONG_MAX &&
                    ir_evaluate_constant(
                        function,
                        instruction->result_value,
                        bindings,
                        binding_count,
                        &value
                    ) &&
                    ir_normalize_integer(
                        instruction->type.name,
                        instruction->type.pointer_depth,
                        instruction->type.is_array,
                        &value
                    )
                ) {
                    if (ir_binding_set(
                            bindings,
                            &binding_count,
                            binding_capacity,
                            instruction->symbol_name,
                            1,
                            value
                        )) {
                        free(bindings);
                        return 1;
                    }
                } else if (ir_binding_set(
                               bindings,
                               &binding_count,
                               binding_capacity,
                               instruction->symbol_name,
                               0,
                               0
                           )) {
                    free(bindings);
                    return 1;
                }
                break;

            case IR_INSTRUCTION_ASSIGNMENT:
                ir_mark_value_constants(
                    function,
                    instruction->right_value,
                    bindings,
                    binding_count
                );
                if (
                    function->values[instruction->left_value].opcode ==
                    IR_VALUE_LOAD &&
                    ir_binding_find(
                        bindings,
                        binding_count,
                        function->values[instruction->left_value].name
                    ) != ULONG_MAX &&
                    ir_evaluate_constant(
                        function,
                        instruction->right_value,
                        bindings,
                        binding_count,
                        &value
                    ) &&
                    ir_normalize_integer(
                        instruction->type.name,
                        instruction->type.pointer_depth,
                        instruction->type.is_array,
                        &value
                    )
                ) {
                    if (ir_binding_set(
                            bindings,
                            &binding_count,
                            binding_capacity,
                            function->values[
                                instruction->left_value
                            ].name,
                            1,
                            value
                        )) {
                        free(bindings);
                        return 1;
                    }
                } else if (
                    function->values[instruction->left_value].opcode ==
                    IR_VALUE_LOAD &&
                    ir_binding_find(
                        bindings,
                        binding_count,
                        function->values[instruction->left_value].name
                    ) != ULONG_MAX
                ) {
                    if (ir_binding_set(
                            bindings,
                            &binding_count,
                            binding_capacity,
                            function->values[
                                instruction->left_value
                            ].name,
                            0,
                            0
                        )) {
                        free(bindings);
                        return 1;
                    }
                } else {
                    ir_bindings_invalidate(bindings, binding_count);
                }
                if (
                    instruction->right_value < function->value_count &&
                    function->values[
                        instruction->right_value
                    ].opcode == IR_VALUE_CALL &&
                    ir_call_may_alias_locals(
                        function,
                        &function->values[
                            instruction->right_value
                        ]
                    )
                ) {
                    ir_bindings_invalidate(bindings, binding_count);
                }
                break;

            case IR_INSTRUCTION_MULTI_ASSIGNMENT:
            {
                unsigned long target_count =
                    instruction->operand_count - 1;
                ir_value *call = &function->values[
                    instruction->operand_values[target_count]
                ];
                uint64_t *results = NULL;
                unsigned long result_count = 0;
                int evaluated;

                ir_mark_value_constants(
                    function,
                    instruction->result_value,
                    bindings,
                    binding_count
                );
                evaluated = ir_evaluate_pure_call(
                    module,
                    function,
                    call,
                    bindings,
                    binding_count,
                    &results,
                    &result_count
                );
                if (evaluated < 0) {
                    free(bindings);
                    return 1;
                }
                if (
                    evaluated &&
                    result_count == target_count
                ) {
                    instruction->constant_values = results;
                    instruction->constant_value_count = result_count;
                    for (unsigned long target_index = 0;
                         target_index < target_count;
                         target_index++)
                    {
                        const ir_value *target = &function->values[
                            instruction->operand_values[target_index]
                        ];
                        unsigned long binding = ir_binding_find(
                            bindings,
                            binding_count,
                            target->name
                        );
                        uint64_t target_value =
                            results[target_index];
                        if (
                            target->opcode == IR_VALUE_LOAD &&
                            binding != ULONG_MAX &&
                            ir_normalize_integer(
                                target->type.name,
                                target->type.pointer_depth,
                                target->type.is_array,
                                &target_value
                            )
                        ) {
                            bindings[binding].known = 1;
                            bindings[binding].value = target_value;
                        } else if (binding != ULONG_MAX) {
                            bindings[binding].known = 0;
                        }
                    }
                } else {
                    free(results);
                    for (unsigned long target_index = 0;
                         target_index < target_count;
                         target_index++)
                    {
                        const ir_value *target = &function->values[
                            instruction->operand_values[target_index]
                        ];
                        unsigned long binding = ir_binding_find(
                            bindings,
                            binding_count,
                            target->name
                        );
                        if (binding != ULONG_MAX) {
                            bindings[binding].known = 0;
                        }
                    }
                    if (ir_call_may_alias_locals(function, call)) {
                        ir_bindings_invalidate(bindings, binding_count);
                    }
                }
                break;
            }

            case IR_INSTRUCTION_EXPRESSION:
                if (instruction->result_value != ULONG_MAX) {
                    ir_mark_value_constants(
                        function,
                        instruction->result_value,
                        bindings,
                        binding_count
                    );
                    const ir_value *expression =
                        &function->values[instruction->result_value];
                    if (
                        expression->opcode == IR_VALUE_CALL &&
                        ir_call_may_alias_locals(function, expression)
                    ) {
                        ir_bindings_invalidate(bindings, binding_count);
                    }
                }
                break;

            case IR_INSTRUCTION_ASSEMBLY:
                ir_bindings_invalidate(bindings, binding_count);
                break;

            case IR_INSTRUCTION_RETURN:
                for (unsigned long value_index = 0;
                     value_index < instruction->operand_count;
                     value_index++)
                {
                    ir_mark_value_constants(
                        function,
                        instruction->operand_values[value_index],
                        bindings,
                        binding_count
                    );
                }
                break;
        }
    }

    free(bindings);
    return 0;
}

static int
ir_optimize_function(
    ir_module *module,
    ir_function *function
)
{
    for (unsigned long index = 0;
         index < function->value_count;
         index++)
    {
        ir_value *value = &function->values[index];
        ir_fold_value(function, value);

        if (
            value->is_constant &&
            value->opcode != IR_VALUE_INTEGER &&
            ir_append_constant(
                module,
                value->source,
                value->constant_value
            )
        ) {
            return 1;
        }
    }

    if (ir_optimize_straight_line(module, function)) {
        return 1;
    }

    for (unsigned long index = 0;
         index < function->block_count;
         index++)
    {
        ir_basic_block *block = &function->blocks[index];
        ir_terminator *terminator = &block->terminator;

        if (
            terminator->kind != IR_TERMINATOR_BRANCH ||
            terminator->condition_value >= function->value_count
        ) {
            continue;
        }

        const ir_value *condition =
            &function->values[terminator->condition_value];
        if (condition->is_constant) {
            unsigned long target = condition->constant_value
                ? terminator->true_block
                : terminator->false_block;

            *terminator = (ir_terminator) {
                .kind = IR_TERMINATOR_JUMP,
                .true_block = target
            };
        }
    }

    return 0;
}


ir_result
ir_lower(
    ast *program,
    int optimize
)
{
    ir_module *module;
    ir_context context;

    if (!program || program->type != AST_PROGRAM) {
        return (ir_result) {
            .error = 1,
            .error_text = ir_copy_text("IR lowering requires a program AST")
        };
    }

    module = calloc(1, sizeof(*module));
    if (!module) {
        return (ir_result) {
            .error = 1,
            .error_text = ir_copy_text("failed to allocate IR module")
        };
    }

    module->source_program = program;
    context = (ir_context) {
        .program = program,
        .module = module
    };

    for (unsigned long index = 0;
         index < program->program.structs.count;
         index++)
    {
        ast *declaration = program->program.structs.values[index];
        if (
            !declaration->imported &&
            ir_mark_declaration(&context, declaration)
        ) {
            goto allocation_error;
        }
    }

    for (unsigned long index = 0;
         index < program->program.globals.count;
         index++)
    {
        ast *declaration = program->program.globals.values[index];
        if (
            !declaration->imported &&
            ir_mark_declaration(&context, declaration)
        ) {
            goto allocation_error;
        }
    }

    for (unsigned long index = 0;
         index < program->program.functions.count;
         index++)
    {
        ast *declaration = program->program.functions.values[index];
        if (
            !declaration->imported &&
            ir_mark_declaration(&context, declaration)
        ) {
            goto allocation_error;
        }
    }

    if (ir_lower_functions(module)) {
        goto allocation_error;
    }

    if (optimize) {
        for (unsigned long index = 0;
             index < module->lowered_function_count;
             index++)
        {
            if (ir_optimize_function(
                    module,
                    &module->lowered_functions[index]
                )) {
                goto allocation_error;
            }
        }

        for (unsigned long index = 0;
             index < module->globals.count;
             index++)
        {
            ast *global = module->globals.values[index];
            if (ir_collect_constants(module, global->variable.value)) {
                goto allocation_error;
            }
        }
    }

    return (ir_result) {
        .error = 0,
        .module = module
    };

allocation_error:
    ir_module_free(module);
    return (ir_result) {
        .error = 1,
        .error_text = ir_copy_text("failed to lower program to IR")
    };
}

void
ir_module_free(
    ir_module *module
)
{
    if (!module) {
        return;
    }

    free(module->structs.values);
    free(module->globals.values);
    free(module->functions.values);
    for (unsigned long index = 0;
         index < module->lowered_function_count;
         index++)
    {
        ir_function *function = &module->lowered_functions[index];
        for (unsigned long block_index = 0;
             block_index < function->block_count;
             block_index++)
        {
            for (unsigned long instruction_index = 0;
                 instruction_index <
                    function->blocks[block_index].instruction_count;
                 instruction_index++)
            {
                ir_instruction_free(
                    &function->blocks[block_index]
                        .instructions[instruction_index]
                );
            }
            free(function->blocks[block_index].instructions);
        }
        free(function->blocks);
        for (unsigned long value_index = 0;
             value_index < function->value_count;
             value_index++)
        {
            free(function->values[value_index].arguments);
        }
        free(function->values);
    }
    free(module->lowered_functions);
    free(module->constants);
    free(module);
}

void
ir_result_free(
    ir_result *result
)
{
    if (!result) {
        return;
    }

    free(result->error_text);
    ir_module_free(result->module);
    result->error_text = NULL;
    result->module = NULL;
}
