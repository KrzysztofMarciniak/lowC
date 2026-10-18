#include "expression.h"
#include "type.h"

#include "../../output.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/*
 * System V AMD64 integer/pointer argument registers.
 *
 * Arguments are passed in this order:
 *
 *     rdi
 *     rsi
 *     rdx
 *     rcx
 *     r8
 *     r9
 */
static const char *argument_registers[] =
{
    "rdi",
    "rsi",
    "rdx",
    "rcx",
    "r8",
    "r9"
};


static unsigned long
expression_pointee_size(
    codegen_x86_64_context *context,
    ast *expression
)
{
    codegen_x86_64_local *local;


    if (
        !expression ||
        expression->type != AST_IDENTIFIER
    ){
        return 0;
    }


    local =
        codegen_x86_64_find_local(
            context,
            expression->identifier.name
        );


    if (!local) {
        return 0;
    }

    if (local->array_count != 0) {
        return codegen_x86_64_context_type_size(
            context,
            local->type,
            local->pointer_depth
        );
    }

    if (local->pointer_depth == 0) {
        return 0;
    }

    return codegen_x86_64_context_type_size(
            context,
            local->type,
            local->pointer_depth - 1
        );
}

static ast *
expression_struct_field(
    codegen_x86_64_context *context,
    ast *expression,
    int pointer_member
)
{
    const char *type;
    unsigned long pointer_depth;
    ast *structure;
    const char *field_name;

    if (!expression) {
        return NULL;
    }

    if (pointer_member) {
        type = NULL;
        pointer_depth = 0;
        if (!codegen_x86_64_expression_type(
                context,
                expression->pointer_member.object,
                &type,
                &pointer_depth
            ) ||
            pointer_depth == 0) {
            return NULL;
        }
        pointer_depth--;
        field_name = expression->pointer_member.name;
    } else {
        type = NULL;
        pointer_depth = 0;
        if (!codegen_x86_64_expression_type(
                context,
                expression->member.object,
                &type,
                &pointer_depth
            ) ||
            pointer_depth != 0) {
            return NULL;
        }
        field_name = expression->member.name;
    }

    structure = codegen_x86_64_find_struct(context, type);
    return codegen_x86_64_find_struct_field(structure, field_name);
}

int
codegen_x86_64_expression_type(
    codegen_x86_64_context *context,
    ast *expression,
    const char **type,
    unsigned long *pointer_depth
)
{
    codegen_x86_64_local *local;

    if (!context || !expression || !type || !pointer_depth) {
        return 0;
    }

    switch (expression->type) {
        case AST_IDENTIFIER:
            local = codegen_x86_64_find_local(
                context,
                expression->identifier.name
            );
            if (!local) {
                return 0;
            }
            *type = local->type;
            *pointer_depth = local->pointer_depth;
            return 1;

        case AST_INTEGER:
            *type = "s64";
            *pointer_depth = 0;
            return 1;

        case AST_UNARY:
            if (!codegen_x86_64_expression_type(
                    context,
                    expression->unary.operand,
                    type,
                    pointer_depth
                )) {
                return 0;
            }
            if (expression->unary.operator == AST_UNARY_ADDRESS) {
                (*pointer_depth)++;
            } else if (
                expression->unary.operator == AST_UNARY_DEREFERENCE
            ) {
                if (*pointer_depth == 0) {
                    return 0;
                }
                (*pointer_depth)--;
            } else if (expression->unary.operator == AST_UNARY_NOT) {
                *type = "u1";
                *pointer_depth = 0;
            }
            return 1;

        case AST_BINARY:
            if (
                expression->binary.operator == AST_BINARY_AND ||
                expression->binary.operator == AST_BINARY_OR ||
                expression->binary.operator == AST_BINARY_EQUAL ||
                expression->binary.operator == AST_BINARY_NOT_EQUAL ||
                expression->binary.operator == AST_BINARY_LESS ||
                expression->binary.operator == AST_BINARY_LESS_EQUAL ||
                expression->binary.operator == AST_BINARY_GREATER ||
                expression->binary.operator == AST_BINARY_GREATER_EQUAL
            ) {
                *type = "u1";
                *pointer_depth = 0;
                return 1;
            }
            if (
                expression->binary.operator == AST_BINARY_ADD ||
                expression->binary.operator == AST_BINARY_SUBTRACT
            ) {
                if (codegen_x86_64_expression_type(
                        context,
                        expression->binary.left,
                        type,
                        pointer_depth
                    ) &&
                    *pointer_depth != 0) {
                    return 1;
                }
                if (codegen_x86_64_expression_type(
                        context,
                        expression->binary.right,
                        type,
                        pointer_depth
                    ) &&
                    *pointer_depth != 0) {
                    return 1;
                }
            }
            *type = "s64";
            *pointer_depth = 0;
            return 1;

        case AST_INDEX:
            if (!codegen_x86_64_expression_type(
                    context,
                    expression->index.object,
                    type,
                    pointer_depth
                )) {
                return 0;
            }
            if (*pointer_depth != 0) {
                (*pointer_depth)--;
            }
            return 1;

        case AST_MEMBER:
        case AST_POINTER_MEMBER:
        {
            ast *field = expression_struct_field(
                context,
                expression,
                expression->type == AST_POINTER_MEMBER
            );
            if (!field) {
                return 0;
            }
            *type = field->struct_field.type;
            *pointer_depth = field->struct_field.pointer_depth;
            return 1;
        }

        case AST_ASSIGNMENT:
            return codegen_x86_64_expression_type(
                context,
                expression->assignment.left,
                type,
                pointer_depth
            );

        case AST_CALL:
            if (
                !context->functions ||
                !expression->call.function ||
                expression->call.function->type != AST_IDENTIFIER
            ) {
                return 0;
            }
            for (unsigned long index = 0;
                 index < context->functions->count;
                 index++)
            {
                ast *function = context->functions->values[index];
                if (strcmp(
                        function->function.name,
                        expression->call.function->identifier.name
                    ) == 0) {
                    *type = function->function.return_type;
                    *pointer_depth =
                        function->function.return_pointer_depth;
                    return 1;
                }
            }
            return 0;

        default:
            return 0;
    }
}


/*
 * Emit a load from the address currently stored in rax.
 *
 * The loaded value is returned in rax.
 *
 * Values smaller than 64 bits are explicitly extended to
 * 64 bits so the rest of the expression evaluator can use
 * a single register representation.
 */
static void
emit_load_from_rax(
    codegen_x86_64_context *context,
    const char *type,
    unsigned long pointer_depth
)
{
    unsigned long size;


    /*
     * Any pointer is an eight-byte value on x86-64.
     */
    size =
        codegen_x86_64_context_type_size(
            context,
            type,
            pointer_depth
        );


    if (size >= 8){

        codegen_output_write(
            context->output,
            "    mov rax, QWORD PTR [rax]\n"
        );

        return;
    }


    if (
        codegen_x86_64_type_signed(
            type,
            pointer_depth
        )
    ){
        codegen_output_printf(
            context->output,
            "    movsx rax, %s [rax]\n",
            codegen_x86_64_type_operand_size(
                type,
                pointer_depth
            )
        );
    }
    else {

        codegen_output_printf(
            context->output,
            "    movzx rax, %s [rax]\n",
            codegen_x86_64_type_operand_size(
                type,
                pointer_depth
            )
        );
    }
}


/*
 * Emit an integer constant.
 */
static void
emit_integer(
    codegen_x86_64_context *context,
    ast *expression
)
{
    codegen_output_printf(
        context->output,
        "    mov rax, %s\n",
        expression->integer.value
    );
}


/*
 * Emit loading of a local variable.
 *
 * The local's address is first placed in rax:
 *
 *     lea rax, [rbp-8]
 *
 * Then the value is loaded from that address using the
 * local's actual semantic type.
 */
static void
emit_variable(
    codegen_x86_64_context *context,
    ast *expression
)
{
    codegen_x86_64_local *local;


    local =
        codegen_x86_64_find_local(
            context,
            expression->identifier.name
        );


    if (!local){
        /*
         * Semantic analysis should normally guarantee
         * that this cannot happen.
         */
        return;
    }


    /*
     * ------------------------------------------------------------
     * Load local variable
     *
     * source:
     *
     *     <type> <name>
     *
     * The local is addressed relative to rbp and then loaded
     * according to its actual type.
     * ------------------------------------------------------------
     */
    codegen_output_printf(
        context->output,
        "    # load %s\n",
        local->name
    );


    codegen_output_printf(
        context->output,
        "    # type: %s",
        local->type
    );


    if (local->pointer_depth != 0){
        unsigned long pointer;


        for (
            pointer = 0;
            pointer < local->pointer_depth;
            pointer++
        ){
            codegen_output_write(
                context->output,
                " *"
            );
        }
    }


    codegen_output_write(
        context->output,
        "\n"
    );


    codegen_output_printf(
        context->output,
        "    # stack location: [rbp%+ld]\n",
        local->offset
    );


    codegen_output_printf(
        context->output,
        "    lea rax, [rbp%+ld]\n",
        local->offset
    );


    /*
     * An array expression evaluates to the address of its
     * first element. Do not load the first element here.
     */
    if (local->array_count != 0)
    {
        return;
    }


    emit_load_from_rax(
        context,
        local->type,
        local->pointer_depth
    );
}


/*
 * Emit a binary arithmetic or comparison operation.
 *
 * The left operand is evaluated first and preserved while
 * the right operand is evaluated.
 *
 * After evaluation:
 *
 *     rax = left
 *     rcx = right
 */
static void
emit_binary(
    codegen_x86_64_context *context,
    ast *expression
)
{
    unsigned long left_pointee_size;
    unsigned long right_pointee_size;


    /*
     * ------------------------------------------------------------
     * Evaluate left operand
     * ------------------------------------------------------------
     */
    codegen_x86_64_expression(
        context,
        expression->binary.left
    );


    codegen_output_write(
        context->output,
        "    push rax\n"
    );

    context->stack_depth += 8;


    /*
     * ------------------------------------------------------------
     * Evaluate right operand
     * ------------------------------------------------------------
     */
    codegen_x86_64_expression(
        context,
        expression->binary.right
    );


    /*
     * rax currently contains the right operand.
     *
     * Move it to rcx, then restore the left operand into rax.
     */
    codegen_output_write(
        context->output,
        "    mov rcx, rax\n"
        "    pop rax\n"
    );

    context->stack_depth -= 8;


    left_pointee_size =
        expression_pointee_size(
            context,
            expression->binary.left
        );


    right_pointee_size =
        expression_pointee_size(
            context,
            expression->binary.right
        );


    switch (expression->binary.operator){

        case AST_BINARY_ADD:

            if (left_pointee_size > 1){
                codegen_output_printf(
                    context->output,
                    "    imul rcx, %lu\n",
                    left_pointee_size
                );
            }
            else if (right_pointee_size > 1){
                codegen_output_printf(
                    context->output,
                    "    imul rax, %lu\n",
                    right_pointee_size
                );
            }

            codegen_output_write(
                context->output,
                "    add rax, rcx\n"
            );

            return;


        case AST_BINARY_SUBTRACT:

            if (
                left_pointee_size > 0 &&
                left_pointee_size == right_pointee_size
            ){
                /*
                 * pointer - pointer => element distance
                 */
                codegen_output_write(
                    context->output,
                    "    sub rax, rcx\n"
                );

                codegen_output_printf(
                    context->output,
                    "    mov rcx, %lu\n"
                    "    cqo\n"
                    "    idiv rcx\n",
                    left_pointee_size
                );

                return;
            }


            if (left_pointee_size > 1){
                codegen_output_printf(
                    context->output,
                    "    imul rcx, %lu\n",
                    left_pointee_size
                );
            }


            codegen_output_write(
                context->output,
                "    sub rax, rcx\n"
            );

            return;


        case AST_BINARY_MULTIPLY:

            codegen_output_write(
                context->output,
                "    imul rax, rcx\n"
            );

            return;


        case AST_BINARY_DIVIDE:

            /*
             * Signed division is currently used here.
             *
             * Semantic signedness handling can be added once
             * expression types are carried directly into this
             * backend.
             */
            codegen_output_write(
                context->output,
                "    cqo\n"
                "    idiv rcx\n"
            );

            return;


        case AST_BINARY_MODULO:

            codegen_output_write(
                context->output,
                "    cqo\n"
                "    idiv rcx\n"
                "    mov rax, rdx\n"
            );

            return;


        case AST_BINARY_EQUAL:

            codegen_output_write(
                context->output,
                "    cmp rax, rcx\n"
                "    sete al\n"
                "    movzx rax, al\n"
            );

            return;


        case AST_BINARY_NOT_EQUAL:

            codegen_output_write(
                context->output,
                "    cmp rax, rcx\n"
                "    setne al\n"
                "    movzx rax, al\n"
            );

            return;


        case AST_BINARY_LESS:

            codegen_output_write(
                context->output,
                "    cmp rax, rcx\n"
                "    setl al\n"
                "    movzx rax, al\n"
            );

            return;


        case AST_BINARY_LESS_EQUAL:

            codegen_output_write(
                context->output,
                "    cmp rax, rcx\n"
                "    setle al\n"
                "    movzx rax, al\n"
            );

            return;


        case AST_BINARY_GREATER:

            codegen_output_write(
                context->output,
                "    cmp rax, rcx\n"
                "    setg al\n"
                "    movzx rax, al\n"
            );

            return;


        case AST_BINARY_GREATER_EQUAL:

            codegen_output_write(
                context->output,
                "    cmp rax, rcx\n"
                "    setge al\n"
                "    movzx rax, al\n"
            );

            return;


        /*
         * Logical operators must short-circuit, so they are
         * emitted by separate functions.
         */
        case AST_BINARY_AND:
        case AST_BINARY_OR:

            return;
    }
}


/*
 * Emit short-circuit logical AND.
 *
 * Semantics:
 *
 *     left && right
 */
static void
emit_logical_and(
    codegen_x86_64_context *context,
    ast *expression
)
{
    unsigned long false_label;
    unsigned long end_label;


    false_label =
        context->label_count++;


    end_label =
        context->label_count++;


    /*
     * ------------------------------------------------------------
     * Evaluate left side
     * ------------------------------------------------------------
     */
    codegen_x86_64_expression(
        context,
        expression->binary.left
    );


    codegen_output_write(
        context->output,
        "    cmp rax, 0\n"
    );


    codegen_output_printf(
        context->output,
        "    je .L%lu\n",
        false_label
    );


    /*
     * ------------------------------------------------------------
     * Evaluate right side
     * ------------------------------------------------------------
     */
    codegen_x86_64_expression(
        context,
        expression->binary.right
    );


    codegen_output_write(
        context->output,
        "    cmp rax, 0\n"
    );


    codegen_output_printf(
        context->output,
        "    je .L%lu\n"
        "    mov rax, 1\n"
        "    jmp .L%lu\n"
        "\n"
        ".L%lu:\n"
        "    mov rax, 0\n"
        "\n"
        ".L%lu:\n",
        false_label,
        end_label,
        false_label,
        end_label
    );
}


/*
 * Emit short-circuit logical OR.
 *
 * Semantics:
 *
 *     left || right
 */
static void
emit_logical_or(
    codegen_x86_64_context *context,
    ast *expression
)
{
    unsigned long true_label;
    unsigned long end_label;


    true_label =
        context->label_count++;


    end_label =
        context->label_count++;


    /*
     * ------------------------------------------------------------
     * Evaluate left side
     * ------------------------------------------------------------
     */
    codegen_x86_64_expression(
        context,
        expression->binary.left
    );


    codegen_output_write(
        context->output,
        "    cmp rax, 0\n"
    );


    codegen_output_printf(
        context->output,
        "    jne .L%lu\n",
        true_label
    );


    /*
     * ------------------------------------------------------------
     * Evaluate right side
     * ------------------------------------------------------------
     */
    codegen_x86_64_expression(
        context,
        expression->binary.right
    );


    codegen_output_write(
        context->output,
        "    cmp rax, 0\n"
    );


    codegen_output_printf(
        context->output,
        "    jne .L%lu\n"
        "    mov rax, 0\n"
        "    jmp .L%lu\n"
        "\n"
        ".L%lu:\n"
        "    mov rax, 1\n"
        "\n"
        ".L%lu:\n",
        true_label,
        end_label,
        true_label,
        end_label
    );
}


/*
 * Emit a unary expression.
 */
static void
emit_unary(
    codegen_x86_64_context *context,
    ast *expression
)
{
    switch (expression->unary.operator){

        case AST_UNARY_PLUS:

            codegen_x86_64_expression(
                context,
                expression->unary.operand
            );

            return;


        case AST_UNARY_MINUS:

            codegen_x86_64_expression(
                context,
                expression->unary.operand
            );

            codegen_output_write(
                context->output,
                "    neg rax\n"
            );

            return;


        case AST_UNARY_NOT:

            codegen_x86_64_expression(
                context,
                expression->unary.operand
            );

            codegen_output_write(
                context->output,
                "    cmp rax, 0\n"
                "    sete al\n"
                "    movzx rax, al\n"
            );

            return;


        case AST_UNARY_ADDRESS:

            codegen_x86_64_lvalue_address(
                context,
                expression->unary.operand
            );

            return;


        case AST_UNARY_DEREFERENCE:
        {
            const char *type;
            unsigned long pointer_depth;

            /*
             * The operand evaluates to an address.
             */
            codegen_x86_64_expression(
                context,
                expression->unary.operand
            );

            if (codegen_x86_64_expression_type(
                    context,
                    expression,
                    &type,
                    &pointer_depth
                )) {
                emit_load_from_rax(context, type, pointer_depth);
            }

            return;
        }
    }
}


/*
 * Emit an array or pointer index.
 *
 * The resulting value is loaded from:
 *
 *     base + index * element_size
 */
static void
emit_index(
    codegen_x86_64_context *context,
    ast *expression
)
{
    const char *type;
    unsigned long pointer_depth;

    /*
     * First calculate the address of the element.
     */
    codegen_x86_64_lvalue_address(
        context,
        expression
    );


    if (codegen_x86_64_expression_type(
            context,
            expression,
            &type,
            &pointer_depth
        )) {
        emit_load_from_rax(context, type, pointer_depth);
    }
}


/*
 * Emit a function call.
 *
 * This currently implements the System V AMD64 integer/pointer
 * calling convention for the first six arguments.
 */
static void
emit_call(
    codegen_x86_64_context *context,
    ast *expression
)
{
    unsigned long count;
    unsigned long index;
    int needs_alignment;

    ast *function;


    if (!expression){
        return;
    }


    count =
        expression->call.arguments.count;


    function =
        expression->call.function;


    if (count > 6){
        /*
         * Stack arguments can be added later.
         */
        return;
    }


    /*
     * ------------------------------------------------------------
     * Evaluate arguments
     * ------------------------------------------------------------
     *
     * Arguments are evaluated from right to left and temporarily
     * stored on the stack.
     */
    for (
        index = count;
        index > 0;
        index--
    ){
        codegen_x86_64_expression(
            context,
            expression->call.arguments.values[
                index - 1
            ]
        );


        codegen_output_write(
            context->output,
            "    push rax\n"
        );


        context->stack_depth += 8;
    }


    /*
     * ------------------------------------------------------------
     * Move arguments into ABI registers
     * ------------------------------------------------------------
     */
    for (
        index = 0;
        index < count;
        index++
    ){
        codegen_output_printf(
            context->output,
            "    pop %s\n",
            argument_registers[index]
        );


        context->stack_depth -= 8;
    }


    needs_alignment = context->stack_depth % 16 != 0;

    if (
        function &&
        function->type == AST_IDENTIFIER
    ){
        if (needs_alignment) {
            codegen_output_write(
                context->output,
                "    sub rsp, 8\n"
            );
        }

        codegen_output_printf(
            context->output,
            "    call %s\n",
            function->identifier.name
        );

        if (needs_alignment) {
            codegen_output_write(
                context->output,
                "    add rsp, 8\n"
            );
        }
    }
}

static int
expression_constant_value(
    ast *expression,
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
            if (!expression_constant_value(
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
            if (!expression_constant_value(
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

            if (!expression_constant_value(
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
                    if (right == 0 ||
                        ((int64_t)left == INT64_MIN &&
                         (int64_t)right == -1)) {
                        return 0;
                    }
                    *value = (uint64_t)((int64_t)left / (int64_t)right);
                    return 1;
                case AST_BINARY_MODULO:
                    if (right == 0 ||
                        ((int64_t)left == INT64_MIN &&
                         (int64_t)right == -1)) {
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


/*
 * Generate the address of an lvalue.
 *
 * The resulting address is placed in rax.
 */
void
codegen_x86_64_lvalue_address(
    codegen_x86_64_context *context,
    ast *expression
)
{
    codegen_x86_64_local *local;

    unsigned long element_size;


    if (
        !context ||
        !expression
    ){
        return;
    }


    switch (expression->type){

        case AST_IDENTIFIER:

            local =
                codegen_x86_64_find_local(
                    context,
                    expression->identifier.name
                );


            if (!local){
                return;
            }


            /*
             * Local variables are addressed relative to rbp.
             */
            codegen_output_printf(
                context->output,
                "    lea rax, [rbp%+ld]\n",
                local->offset
            );

            return;


        case AST_UNARY:

            if (
                expression->unary.operator ==
                AST_UNARY_DEREFERENCE
            ){
                /*
                 * The operand already evaluates to an address.
                 */
                codegen_x86_64_expression(
                    context,
                    expression->unary.operand
                );

                return;
            }

            return;


        case AST_INDEX:

            /*
             * ----------------------------------------------------
             * Calculate base address
             * ----------------------------------------------------
             */
            codegen_x86_64_expression(
                context,
                expression->index.object
            );


            /*
             * Preserve the base address while evaluating the
             * index expression.
             */
            codegen_output_write(
                context->output,
                "    push rax\n"
            );

            context->stack_depth += 8;


            /*
             * ----------------------------------------------------
             * Calculate index
             * ----------------------------------------------------
             */
            codegen_x86_64_expression(
                context,
                expression->index.index
            );


            /*
             * rax = index
             * rcx = index
             * rax = base address
             */
            codegen_output_write(
                context->output,
                "    mov rcx, rax\n"
                "    pop rax\n"
            );

            context->stack_depth -= 8;


            element_size = 8;
            if (expression_pointee_size(
                    context,
                    expression->index.object
                ) != 0) {
                element_size = expression_pointee_size(
                    context,
                    expression->index.object
                );
            }


            codegen_output_printf(
                context->output,
                "    imul rcx, %lu\n",
                element_size
            );


            codegen_output_write(
                context->output,
                "    add rax, rcx\n"
            );

            return;


        case AST_MEMBER:
        {
            const char *type;
            unsigned long pointer_depth;
            unsigned long offset;

            /*
             * Calculate the address of the containing object.
             */
            codegen_x86_64_lvalue_address(
                context,
                expression->member.object
            );


            if (
                codegen_x86_64_expression_type(
                    context,
                    expression->member.object,
                    &type,
                    &pointer_depth
                ) &&
                pointer_depth == 0 &&
                codegen_x86_64_struct_field_offset(
                    context,
                    type,
                    expression->member.name,
                    &offset
                ) == 0 &&
                offset != 0
            ) {
                codegen_output_printf(
                    context->output,
                    "    add rax, %lu\n",
                    offset
                );
            }
            return;
        }

        case AST_POINTER_MEMBER:
        {
            const char *type;
            unsigned long pointer_depth;
            unsigned long offset;

            /*
             * The object expression evaluates to the structure
             * address.
             */
            codegen_x86_64_expression(
                context,
                expression->pointer_member.object
            );


            if (
                codegen_x86_64_expression_type(
                    context,
                    expression->pointer_member.object,
                    &type,
                    &pointer_depth
                ) &&
                pointer_depth != 0 &&
                codegen_x86_64_struct_field_offset(
                    context,
                    type,
                    expression->pointer_member.name,
                    &offset
                ) == 0 &&
                offset != 0
            ) {
                codegen_output_printf(
                    context->output,
                    "    add rax, %lu\n",
                    offset
                );
            }
            return;
        }


        default:

            return;
    }
}


/*
 * Generate an expression.
 *
 * The resulting value is placed in rax.
 */
void
codegen_x86_64_expression(
    codegen_x86_64_context *context,
    ast *expression
)
{
    uint64_t constant;

    if (
        !context ||
        !expression
    ){
        return;
    }

    if (
        context->optimize &&
        expression->type != AST_INTEGER &&
        expression_constant_value(expression, &constant)
    ) {
        codegen_output_printf(
            context->output,
            "    mov rax, %" PRIu64 "\n",
            constant
        );
        return;
    }

    switch (expression->type){

        case AST_INTEGER:

            emit_integer(
                context,
                expression
            );

            return;


        case AST_IDENTIFIER:

            emit_variable(
                context,
                expression
            );

            return;


        case AST_UNARY:

            emit_unary(
                context,
                expression
            );

            return;


        case AST_BINARY:

            switch (expression->binary.operator){

                case AST_BINARY_AND:

                    emit_logical_and(
                        context,
                        expression
                    );

                    return;


                case AST_BINARY_OR:

                    emit_logical_or(
                        context,
                        expression
                    );

                    return;


                default:

                    emit_binary(
                        context,
                        expression
                    );

                    return;
            }


        case AST_CALL:

            emit_call(
                context,
                expression
            );

            return;


        case AST_INDEX:

            emit_index(
                context,
                expression
            );

            return;


        case AST_MEMBER:
        case AST_POINTER_MEMBER:
        {
            const char *type;
            unsigned long pointer_depth;

            codegen_x86_64_lvalue_address(
                context,
                expression
            );

            if (codegen_x86_64_expression_type(
                    context,
                    expression,
                    &type,
                    &pointer_depth
                )) {
                emit_load_from_rax(context, type, pointer_depth);
            }

            return;
        }


        default:

            /*
             * Unsupported AST nodes should normally be rejected
             * before reaching target code generation.
             */
            return;
    }
}
