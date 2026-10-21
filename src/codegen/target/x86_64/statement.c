#include "statement.h"
#include "expression.h"
#include "type.h"

#include "../../output.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
codegen_x86_64_zero_initializer(
    const ast *expression
)
{
    return
        expression != NULL &&
        expression->type == AST_INTEGER &&
        strtoull(expression->integer.value, NULL, 10) == 0;
}


/*
 * Write a simple expression as source text.
 *
 * This is intentionally small.
 *
 * The purpose is not to become a complete source printer.
 * It only handles the expression forms that are useful for
 * readable code-generation comments right now.
 */
static void
emit_expression_source(
    codegen_x86_64_context *context,
    ast *expression
)
{
    if (!context || !expression){
        return;
    }


    switch (expression->type){

        case AST_INTEGER:

            codegen_output_write(
                context->output,
                expression->integer.value
            );

            return;


        case AST_IDENTIFIER:

            codegen_output_write(
                context->output,
                expression->identifier.name
            );

            return;


        case AST_UNARY:

            switch (expression->unary.operator){

                case AST_UNARY_PLUS:

                    codegen_output_write(
                        context->output,
                        "+"
                    );

                    break;

                case AST_UNARY_MINUS:

                    codegen_output_write(
                        context->output,
                        "-"
                    );

                    break;

                case AST_UNARY_NOT:

                    codegen_output_write(
                        context->output,
                        "!"
                    );

                    break;

                case AST_UNARY_ADDRESS:

                    codegen_output_write(
                        context->output,
                        "&"
                    );

                    break;

                case AST_UNARY_DEREFERENCE:

                    codegen_output_write(
                        context->output,
                        "*"
                    );

                    break;
            }


            emit_expression_source(
                context,
                expression->unary.operand
            );

            return;


        case AST_BINARY:

            codegen_output_write(
                context->output,
                "("
            );

            emit_expression_source(
                context,
                expression->binary.left
            );


            switch (expression->binary.operator){

                case AST_BINARY_ADD:
                    codegen_output_write(context->output, " + ");
                    break;

                case AST_BINARY_SUBTRACT:
                    codegen_output_write(context->output, " - ");
                    break;

                case AST_BINARY_MULTIPLY:
                    codegen_output_write(context->output, " * ");
                    break;

                case AST_BINARY_DIVIDE:
                    codegen_output_write(context->output, " / ");
                    break;

                case AST_BINARY_MODULO:
                    codegen_output_write(context->output, " % ");
                    break;

                case AST_BINARY_EQUAL:
                    codegen_output_write(context->output, " == ");
                    break;

                case AST_BINARY_NOT_EQUAL:
                    codegen_output_write(context->output, " != ");
                    break;

                case AST_BINARY_LESS:
                    codegen_output_write(context->output, " < ");
                    break;

                case AST_BINARY_LESS_EQUAL:
                    codegen_output_write(context->output, " <= ");
                    break;

                case AST_BINARY_GREATER:
                    codegen_output_write(context->output, " > ");
                    break;

                case AST_BINARY_GREATER_EQUAL:
                    codegen_output_write(context->output, " >= ");
                    break;

                case AST_BINARY_AND:
                    codegen_output_write(context->output, " && ");
                    break;

                case AST_BINARY_OR:
                    codegen_output_write(context->output, " || ");
                    break;
            }


            emit_expression_source(
                context,
                expression->binary.right
            );


            codegen_output_write(
                context->output,
                ")"
            );

            return;


        case AST_CALL:

            emit_expression_source(
                context,
                expression->call.function
            );

            codegen_output_write(
                context->output,
                "("
            );


            for (
                unsigned long argument = 0;
                argument < expression->call.arguments.count;
                argument++
            ){

                if (argument != 0){
                    codegen_output_write(
                        context->output,
                        ", "
                    );
                }


                emit_expression_source(
                    context,
                    expression->call.arguments.values[argument]
                );
            }


            codegen_output_write(
                context->output,
                ")"
            );

            return;


        case AST_INDEX:

            emit_expression_source(
                context,
                expression->index.object
            );

            codegen_output_write(
                context->output,
                "["
            );

            emit_expression_source(
                context,
                expression->index.index
            );

            codegen_output_write(
                context->output,
                "]"
            );

            return;


        case AST_MEMBER:

            emit_expression_source(
                context,
                expression->member.object
            );

            codegen_output_printf(
                context->output,
                ".%s",
                expression->member.name
            );

            return;


        case AST_POINTER_MEMBER:

            emit_expression_source(
                context,
                expression->pointer_member.object
            );

            codegen_output_printf(
                context->output,
                "->%s",
                expression->pointer_member.name
            );

            return;


        default:

            /*
             * The expression is valid AST, but this small source
             * printer does not have a readable representation for
             * it yet.
             */
            codegen_output_write(
                context->output,
                "<expression>"
            );

            return;
    }
}

/*
 * Emit a store to a local variable.
 *
 * The value is already in rax.
 *
 * The destination type determines the actual store width.
 *
 * Examples:
 *
 *     u1  -> mov BYTE PTR [rbp-8], al
 *     u2 -> mov WORD PTR [rbp-8], ax
 *     u4 -> mov DWORD PTR [rbp-8], eax
 *     u8 -> mov QWORD PTR [rbp-8], rax
 *
 * The x86-64 register returned by codegen_x86_64_type_register()
 * contains the low part of rax with the correct width.
 */
static void
emit_local_store(
    codegen_x86_64_context *context,
    codegen_x86_64_local *local
)
{
    const char *operand_size;
    const char *register_name;


    if (!context || !local){
        return;
    }


    operand_size =
        codegen_x86_64_type_operand_size(
            local->type,
            local->pointer_depth
        );


    register_name =
        codegen_x86_64_type_register(
            local->type,
            local->pointer_depth
        );


    /*
     * A zero-sized/unknown type should not produce invalid
     * assembly.
     */
    if (!operand_size || !register_name){
        return;
    }


    codegen_output_printf(
        context->output,
        "    mov %s [rbp%+ld], %s\n",
        operand_size,
        local->offset,
        register_name
    );
}


/*
 * Generate an assignment.
 *
 * Example:
 *
 *     x = y;
 *
 * The left-hand side is first converted into an address.
 *
 *     rax = address of x
 *
 * The address is saved while the right-hand side is evaluated.
 *
 *     push rax
 *
 * The right-hand side is then evaluated.
 *
 *     rax = value of y
 *
 * Finally the destination address is restored and the value
 * is stored.
 */
static void
emit_assignment(
    codegen_x86_64_context *context,
    ast *statement
)
{
    codegen_x86_64_local *local;
    const char *type;
    const char *operand_size;
    const char *register_name;
    unsigned long pointer_depth;


    /*
     * ------------------------------------------------------------
     * Assignment destination
     *
     * source: left-hand side of assignment
     * ------------------------------------------------------------
     *
     * Generate the address of the destination.
     *
     * rax = destination address
     */
    codegen_x86_64_lvalue_address(
        context,
        statement->assignment.left
    );


    /*
     * Save the destination address while the right-hand side
     * is evaluated.
     */
    codegen_output_write(
        context->output,
        "    push rax\n"
    );

    context->stack_depth += 8;


    /*
     * ------------------------------------------------------------
     * Assignment value
     *
     * source: right-hand side of assignment
     * ------------------------------------------------------------
     *
     * Generate the value.
     *
     * rax = right-hand side value
     */
    codegen_x86_64_expression(
        context,
        statement->assignment.right
    );


    /*
     * Restore the destination address.
     *
     * rcx = destination address
     * rax = value
     */
    codegen_output_write(
        context->output,
        "    pop rcx\n"
    );

    context->stack_depth -= 8;


    /*
     * ------------------------------------------------------------
     * Assignment store
     * ------------------------------------------------------------
     *
     * Simple identifier assignments can use the local's actual
     * type.
     *
     * More complicated lvalues such as:
     *
     *     *pointer = value;
     *     array[index] = value;
     *
     * do not yet carry enough type information into this backend,
     * so they retain the existing 8-byte store for now.
     */
    if (
        statement->assignment.left &&
        statement->assignment.left->type == AST_IDENTIFIER
    ){
        local =
            codegen_x86_64_find_local(
                context,
                statement->assignment.left->identifier.name
            );


        if (local){
            emit_local_store(
                context,
                local
            );

            return;
        }
    }


    if (
        codegen_x86_64_expression_type(
            context,
            statement->assignment.left,
            &type,
            &pointer_depth
        )
    ) {
        operand_size = codegen_x86_64_type_operand_size(
            type,
            pointer_depth
        );
        register_name = codegen_x86_64_type_register(
            type,
            pointer_depth
        );
        if (operand_size && register_name) {
            codegen_output_printf(
                context->output,
                "    mov %s [rcx], %s\n",
                operand_size,
                register_name
            );
            return;
        }
    }

    codegen_output_write(context->output, "    mov QWORD PTR [rcx], rax\n");
}

static unsigned long
align_up(
    unsigned long value,
    unsigned long alignment
)
{
    unsigned long remainder;

    if (alignment == 0)
        return value;

    remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
}

static unsigned long
multi_return_value_size(
    codegen_x86_64_context *context,
    ast *type
)
{
    if (
        type->return_type.pointer_depth == 0 &&
        codegen_x86_64_find_struct(
            context,
            type->return_type.type
        ) != NULL
    ) {
        return codegen_x86_64_context_type_size(
            context,
            type->return_type.type,
            0
        );
    }

    return 8;
}

static unsigned long
multi_return_value_alignment(
    codegen_x86_64_context *context,
    ast *type
)
{
    if (
        type->return_type.pointer_depth == 0 &&
        codegen_x86_64_find_struct(
            context,
            type->return_type.type
        ) != NULL
    ) {
        return codegen_x86_64_context_type_alignment(
            context,
            type->return_type.type,
            0
        );
    }

    return 8;
}

static unsigned long
multi_return_offset(
    codegen_x86_64_context *context,
    ast *function,
    unsigned long return_index
)
{
    unsigned long offset = 0;

    for (unsigned long index = 0; index < return_index; index++) {
        ast *type = function->function.return_types.values[index];
        unsigned long size = multi_return_value_size(context, type);
        unsigned long alignment =
            multi_return_value_alignment(context, type);

        offset = align_up(offset, alignment);
        offset += size;
    }

    {
        ast *type = function->function.return_types.values[return_index];
        unsigned long alignment =
            multi_return_value_alignment(context, type);

        return align_up(offset, alignment);
    }
}

static unsigned long
multi_return_buffer_size(
    codegen_x86_64_context *context,
    ast *function
)
{
    unsigned long size = 0;

    for (
        unsigned long index = 0;
        index < function->function.return_types.count;
        index++
    ) {
        ast *type = function->function.return_types.values[index];
        unsigned long alignment =
            multi_return_value_alignment(context, type);
        unsigned long value_size = multi_return_value_size(context, type);

        size = align_up(size, alignment);
        size += value_size;
    }

    return align_up(size, 16);
}

static void
emit_byte_copy(
    codegen_x86_64_context *context,
    unsigned long size
)
{
    for (unsigned long index = 0; index < size; index++) {
        codegen_output_printf(
            context->output,
            "    mov dl, BYTE PTR [rsi + %lu]\n"
            "    mov BYTE PTR [rdi + %lu], dl\n",
            index,
            index
        );
    }
}

static void
emit_multi_assignment(
    codegen_x86_64_context *context,
    ast *statement
)
{
    ast *call = statement->multi_assignment.value;
    ast *function = NULL;
    unsigned long buffer_size;

    if (
        call->type != AST_CALL ||
        call->call.function == NULL ||
        call->call.function->type != AST_IDENTIFIER ||
        context->functions == NULL
    ) {
        return;
    }

    for (
        unsigned long index = 0;
        index < context->functions->count;
        index++
    ) {
        ast *candidate = context->functions->values[index];

        if (strcmp(
                candidate->function.name,
                call->call.function->identifier.name
            ) == 0) {
            function = candidate;
            break;
        }
    }

    if (
        function == NULL ||
        function->function.return_types.count !=
            statement->multi_assignment.targets.count
    ) {
        return;
    }

    buffer_size = multi_return_buffer_size(context, function);
    if (buffer_size == 0)
        return;

    codegen_output_printf(
        context->output,
        "    sub rsp, %lu\n",
        buffer_size
    );
    context->stack_depth += buffer_size;

    codegen_x86_64_expression(context, call);

    for (
        unsigned long index = 0;
        index < statement->multi_assignment.targets.count;
        index++
    ) {
        ast *target = statement->multi_assignment.targets.values[index];
        ast *return_type = function->function.return_types.values[index];
        const char *target_type;
        unsigned long target_pointer_depth;
        unsigned long target_size;
        unsigned long source_offset = multi_return_offset(
            context,
            function,
            index
        );

        codegen_x86_64_lvalue_address(context, target);
        codegen_output_write(context->output, "    push rax\n");
        context->stack_depth += 8;

        codegen_output_printf(
            context->output,
            "    lea rsi, [rsp + %lu]\n",
            source_offset + 8
        );

        codegen_output_write(
            context->output,
            "    pop rdi\n"
        );
        context->stack_depth -= 8;

        if (
            codegen_x86_64_expression_type(
                context,
                target,
                &target_type,
                &target_pointer_depth
            )
        ) {
            target_size = codegen_x86_64_context_type_size(
                context,
                target_type,
                target_pointer_depth
            );
        } else {
            target_size = codegen_x86_64_context_type_size(
                context,
                return_type->return_type.type,
                return_type->return_type.pointer_depth
            );
        }

        emit_byte_copy(context, target_size);
    }

    codegen_output_printf(
        context->output,
        "    add rsp, %lu\n",
        buffer_size
    );
    context->stack_depth -= buffer_size;
}

static void
emit_ir_multi_assignment(
    codegen_x86_64_context *context,
    const ir_function *lowered_function,
    const ir_instruction *instruction
)
{
    unsigned long target_count = instruction->operand_count - 1;
    const ir_value *call_value = &lowered_function->values[
        instruction->operand_values[target_count]
    ];
    ast *function = NULL;
    unsigned long buffer_size;

    if (
        call_value->opcode != IR_VALUE_CALL ||
        context->functions == NULL
    ) {
        return;
    }

    if (instruction->constant_value_count == target_count) {
        int can_store_constants = 1;

        for (unsigned long index = 0; index < target_count; index++) {
            const ir_value *target = &lowered_function->values[
                instruction->operand_values[index]
            ];
            if (
                target->opcode != IR_VALUE_LOAD ||
                target->type.pointer_depth != 0 ||
                target->type.is_array ||
                codegen_x86_64_type_size(target->type.name, 0) == 0 ||
                !codegen_x86_64_find_local(context, target->name)
            ) {
                can_store_constants = 0;
                break;
            }
        }

        if (can_store_constants) {
            for (unsigned long index = 0; index < target_count; index++) {
                const ir_value *target = &lowered_function->values[
                    instruction->operand_values[index]
                ];
                codegen_x86_64_local *local =
                    codegen_x86_64_find_local(context, target->name);
                codegen_output_printf(
                    context->output,
                    "    mov rax, %" PRIu64 "\n",
                    instruction->constant_values[index]
                );
                emit_local_store(context, local);
            }
            return;
        }
    }

    for (unsigned long index = 0;
         index < context->functions->count;
         index++)
    {
        ast *candidate = context->functions->values[index];
        if (strcmp(candidate->function.name, call_value->name) == 0) {
            function = candidate;
            break;
        }
    }

    if (
        function == NULL ||
        function->function.return_types.count != target_count
    ) {
        return;
    }

    buffer_size = multi_return_buffer_size(context, function);
    if (buffer_size == 0) {
        return;
    }

    codegen_output_printf(
        context->output,
        "    sub rsp, %lu\n",
        buffer_size
    );
    context->stack_depth += buffer_size;

    codegen_x86_64_ir_expression_id(
        context,
        instruction->operand_values[target_count]
    );

    for (unsigned long index = 0; index < target_count; index++) {
        const ir_value *target = &lowered_function->values[
            instruction->operand_values[index]
        ];
        ast *return_type = function->function.return_types.values[index];
        unsigned long source_offset = multi_return_offset(
            context,
            function,
            index
        );
        unsigned long target_size = codegen_x86_64_context_type_size(
            context,
            target->type.name,
            target->type.pointer_depth
        );

        codegen_x86_64_lvalue_address(
            context,
            (ast *)target->source
        );
        codegen_output_write(context->output, "    push rax\n");
        context->stack_depth += 8;
        codegen_output_printf(
            context->output,
            "    lea rsi, [rsp + %lu]\n",
            source_offset + 8
        );
        codegen_output_write(context->output, "    pop rdi\n");
        context->stack_depth -= 8;
        if (target_size == 0) {
            target_size = codegen_x86_64_context_type_size(
                context,
                return_type->return_type.type,
                return_type->return_type.pointer_depth
            );
        }
        emit_byte_copy(context, target_size);
    }

    codegen_output_printf(
        context->output,
        "    add rsp, %lu\n",
        buffer_size
    );
    context->stack_depth -= buffer_size;
}

static void
emit_multi_return(
    codegen_x86_64_context *context,
    const ir_function *lowered_function,
    const ir_instruction *instruction
)
{
    ast *function = context->current_function;

    if (
        function == NULL ||
        !context->has_return_buffer ||
        function->function.return_types.count < 2
    )
        return;

    for (
        unsigned long index = 0;
        index < instruction->operand_count;
        index++
    ) {
        ast *return_type = function->function.return_types.values[index];
        ast *value = (ast *)lowered_function->values[
            instruction->operand_values[index]
        ].source;
        unsigned long size = codegen_x86_64_context_type_size(
            context,
            return_type->return_type.type,
            return_type->return_type.pointer_depth
        );
        unsigned long destination_offset = multi_return_offset(
            context,
            function,
            index
        );
        int is_struct =
            return_type->return_type.pointer_depth == 0 &&
            codegen_x86_64_find_struct(
                context,
                return_type->return_type.type
            ) != NULL;

        if (is_struct) {
            codegen_x86_64_lvalue_address(context, value);
            codegen_output_write(
                context->output,
                "    mov rsi, rax\n"
            );
            codegen_output_printf(
                context->output,
                "    mov rdi, QWORD PTR [rbp%+ld]\n",
                context->return_buffer_offset
            );
            if (destination_offset != 0) {
                codegen_output_printf(
                    context->output,
                    "    add rdi, %lu\n",
                    destination_offset
                );
            }
            emit_byte_copy(context, size);
        } else {
            codegen_x86_64_ir_expression_id(
                context,
                instruction->operand_values[index]
            );
            codegen_output_printf(
                context->output,
                "    mov rdi, QWORD PTR [rbp%+ld]\n",
                context->return_buffer_offset
            );
            if (destination_offset != 0) {
                codegen_output_printf(
                    context->output,
                    "    add rdi, %lu\n",
                    destination_offset
                );
            }

            switch (codegen_x86_64_type_size(
                return_type->return_type.type,
                return_type->return_type.pointer_depth
            )) {
                case 1:
                    codegen_output_write(
                        context->output,
                        codegen_x86_64_type_signed(
                            return_type->return_type.type,
                            return_type->return_type.pointer_depth
                        )
                            ? "    movsx rax, al\n"
                            : "    movzx rax, al\n"
                    );
                    break;
                case 2:
                    codegen_output_write(
                        context->output,
                        codegen_x86_64_type_signed(
                            return_type->return_type.type,
                            return_type->return_type.pointer_depth
                        )
                            ? "    movsx rax, ax\n"
                            : "    movzx rax, ax\n"
                    );
                    break;
                case 4:
                    codegen_output_write(
                        context->output,
                        codegen_x86_64_type_signed(
                            return_type->return_type.type,
                            return_type->return_type.pointer_depth
                        )
                            ? "    movsxd rax, eax\n"
                            : "    mov eax, eax\n"
                    );
                    break;
            }

            codegen_output_write(
                context->output,
                "    mov QWORD PTR [rdi], rax\n"
            );
        }
    }

}

static void
emit_ir_return(
    codegen_x86_64_context *context,
    const ir_function *function,
    const ir_instruction *instruction
)
{
    codegen_x86_64_emit_location(
        context,
        instruction->source
    );
    codegen_output_write(
        context->output,
        "    # return\n"
    );

    if (instruction->operand_count > 1) {
        emit_multi_return(context, function, instruction);
    } else if (instruction->operand_count == 1) {
        codegen_x86_64_ir_expression_id(
            context,
            instruction->operand_values[0]
        );
    }

    codegen_output_printf(
        context->output,
        "    jmp .L%lu\n",
        context->epilogue_label
    );
}

static int
assembly_register_is_callee_saved(
    const char *register_name
)
{
    return
        strcmp(register_name, "rbx") == 0 ||
        strcmp(register_name, "r12") == 0 ||
        strcmp(register_name, "r13") == 0 ||
        strcmp(register_name, "r14") == 0 ||
        strcmp(register_name, "r15") == 0;
}

static void
assembly_save_callee_saved_register(
    codegen_x86_64_context *context,
    const char *register_name,
    const char **saved,
    unsigned long *saved_count
)
{
    if (!assembly_register_is_callee_saved(register_name))
        return;

    for (unsigned long index = 0; index < *saved_count; index++) {
        if (strcmp(saved[index], register_name) == 0)
            return;
    }

    codegen_output_printf(
        context->output,
        "    push %s\n",
        register_name
    );
    context->stack_depth += 8;
    saved[(*saved_count)++] = register_name;
}

static const char *
assembly_output_register(
    ast *operand
)
{
    return operand->assembly_operand.constraint + 1;
}

static const char *
assembly_input_register(
    ast *operand
)
{
    return operand->assembly_operand.constraint;
}

static const char *
assembly_low_register(
    unsigned long size
)
{
    switch (size) {
        case 1: return "cl";
        case 2: return "cx";
        case 4: return "ecx";
        default: return "rcx";
    }
}

static void
emit_assembly_template(
    codegen_x86_64_context *context,
    ast *assembly,
    const ir_instruction *instruction
)
{
    const char *text = instruction
        ? instruction->assembly_template
        : assembly->assembly.template;
    unsigned long output_count = instruction
        ? instruction->output_count
        : assembly->assembly.outputs.count;
    unsigned long input_count = instruction
        ? instruction->input_count
        : assembly->assembly.inputs.count;
    int line_start = 1;

    codegen_output_write(context->output, "\n");

    for (unsigned long index = 0; text[index] != '\0'; index++) {
        char value = text[index];

        if (value == '\\' && text[index + 1] != '\0') {
            index++;
            switch (text[index]) {
                case 'n': value = '\n'; break;
                case 'r': value = '\r'; break;
                case 't': value = '\t'; break;
                case '\\':
                case '"': value = text[index]; break;
                default: value = text[index]; break;
            }
        } else if (value == '%') {
            unsigned long operand_index = 0;

            if (text[index + 1] == '%') {
                index++;
            } else {
                index++;
                while (text[index] >= '0' && text[index] <= '9') {
                    operand_index =
                        operand_index * 10 +
                        (unsigned long)(text[index] - '0');
                    index++;
                }
                index--;

                if (operand_index < output_count) {
                    if (line_start)
                        codegen_output_write(context->output, "    ");
                    codegen_output_write(
                        context->output,
                        instruction
                            ? instruction->output_constraints[operand_index] + 1
                            : assembly_output_register(
                                assembly->assembly.outputs.values[
                                    operand_index
                                ]
                            )
                    );
                } else {
                    operand_index -= output_count;
                    if (operand_index < input_count) {
                        if (line_start)
                            codegen_output_write(context->output, "    ");
                        codegen_output_write(
                            context->output,
                            instruction
                                ? instruction->input_constraints[
                                    operand_index
                                ]
                                : assembly_input_register(
                                    assembly->assembly.inputs.values[
                                        operand_index
                                    ]
                                )
                        );
                    }
                }
                line_start = 0;
                continue;
            }
        }

        if (line_start && value != '\n')
            codegen_output_write(context->output, "    ");

        codegen_output_write(
            context->output,
            (char[2]) { value, '\0' }
        );
        line_start = value == '\n';
    }

    if (!line_start)
        codegen_output_write(context->output, "\n");
}

static void
emit_assembly(
    codegen_x86_64_context *context,
    ast *assembly
)
{
    const char *saved_registers[5];
    unsigned long saved_count = 0;
    unsigned long output_count = assembly->assembly.outputs.count;
    unsigned long input_count = assembly->assembly.inputs.count;
    int stack_padding;

    for (unsigned long index = 0; index < output_count; index++) {
        assembly_save_callee_saved_register(
            context,
            assembly_output_register(
                assembly->assembly.outputs.values[index]
            ),
            saved_registers,
            &saved_count
        );
    }

    for (unsigned long index = 0; index < input_count; index++) {
        assembly_save_callee_saved_register(
            context,
            assembly_input_register(
                assembly->assembly.inputs.values[index]
            ),
            saved_registers,
            &saved_count
        );
    }

    for (
        unsigned long index = 0;
        index < assembly->assembly.clobbers.count;
        index++
    ) {
        ast *clobber = assembly->assembly.clobbers.values[index];

        if (
            strcmp(clobber->string.value, "memory") != 0 &&
            strcmp(clobber->string.value, "cc") != 0
        ) {
            assembly_save_callee_saved_register(
                context,
                clobber->string.value,
                saved_registers,
                &saved_count
            );
        }
    }

    for (unsigned long index = output_count; index > 0; index--) {
        ast *operand = assembly->assembly.outputs.values[index - 1];

        codegen_x86_64_lvalue_address(
            context,
            operand->assembly_operand.expression
        );
        codegen_output_write(context->output, "    push rax\n");
        context->stack_depth += 8;
    }

    for (unsigned long index = input_count; index > 0; index--) {
        ast *operand = assembly->assembly.inputs.values[index - 1];

        codegen_x86_64_expression(
            context,
            operand->assembly_operand.expression
        );
        codegen_output_write(context->output, "    push rax\n");
        context->stack_depth += 8;
    }

    for (unsigned long index = 0; index < input_count; index++) {
        codegen_output_printf(
            context->output,
            "    pop %s\n",
            assembly_input_register(assembly->assembly.inputs.values[index])
        );
        context->stack_depth -= 8;
    }

    stack_padding = context->stack_depth % 16 != 0;
    if (stack_padding) {
        codegen_output_write(context->output, "    sub rsp, 8\n");
        context->stack_depth += 8;
    }

    emit_assembly_template(context, assembly, NULL);

    if (stack_padding) {
        codegen_output_write(context->output, "    add rsp, 8\n");
        context->stack_depth -= 8;
    }

    for (unsigned long index = output_count; index > 0; index--) {
        codegen_output_printf(
            context->output,
            "    push %s\n",
            assembly_output_register(assembly->assembly.outputs.values[index - 1])
        );
        context->stack_depth += 8;
    }

    for (unsigned long index = 0; index < output_count; index++) {
        ast *operand = assembly->assembly.outputs.values[index];
        const char *type;
        unsigned long pointer_depth;
        unsigned long size;

        codegen_output_write(
            context->output,
            "    pop rcx\n"
            "    pop rax\n"
        );
        context->stack_depth -= 16;

        if (
            codegen_x86_64_expression_type(
                context,
                operand->assembly_operand.expression,
                &type,
                &pointer_depth
            )
        ) {
            size = codegen_x86_64_context_type_size(
                context,
                type,
                pointer_depth
            );
            codegen_output_printf(
                context->output,
                "    mov %s [rax], %s\n",
                codegen_x86_64_type_operand_size(type, pointer_depth),
                assembly_low_register(size)
            );
        }
    }

    while (saved_count > 0) {
        codegen_output_printf(
            context->output,
            "    pop %s\n",
            saved_registers[--saved_count]
        );
        context->stack_depth -= 8;
    }
}

static void
emit_ir_assembly(
    codegen_x86_64_context *context,
    const ir_function *function,
    const ir_instruction *instruction
)
{
    const char *saved_registers[5];
    unsigned long saved_count = 0;
    int stack_padding;

    codegen_x86_64_emit_location(context, instruction->source);

    for (unsigned long index = 0;
         index < instruction->output_count;
         index++)
    {
        assembly_save_callee_saved_register(
            context,
            instruction->output_constraints[index] + 1,
            saved_registers,
            &saved_count
        );
    }
    for (unsigned long index = 0;
         index < instruction->input_count;
         index++)
    {
        assembly_save_callee_saved_register(
            context,
            instruction->input_constraints[index],
            saved_registers,
            &saved_count
        );
    }
    for (unsigned long index = 0;
         index < instruction->clobber_count;
         index++)
    {
        if (
            strcmp(instruction->clobbers[index], "memory") != 0 &&
            strcmp(instruction->clobbers[index], "cc") != 0
        ) {
            assembly_save_callee_saved_register(
                context,
                instruction->clobbers[index],
                saved_registers,
                &saved_count
            );
        }
    }

    for (unsigned long index = instruction->output_count; index > 0; index--) {
        const ir_value *value = &function->values[
            instruction->operand_values[index - 1]
        ];
        codegen_x86_64_lvalue_address(
            context,
            (ast *)value->source
        );
        codegen_output_write(context->output, "    push rax\n");
        context->stack_depth += 8;
    }
    for (unsigned long index = instruction->input_count; index > 0; index--) {
        const ir_value *value = &function->values[
            instruction->operand_values[
                instruction->output_count + index - 1
            ]
        ];
        codegen_x86_64_expression(
            context,
            (ast *)value->source
        );
        codegen_output_write(context->output, "    push rax\n");
        context->stack_depth += 8;
    }

    for (unsigned long index = 0;
         index < instruction->input_count;
         index++)
    {
        codegen_output_printf(
            context->output,
            "    pop %s\n",
            instruction->input_constraints[index]
        );
        context->stack_depth -= 8;
    }

    stack_padding = context->stack_depth % 16 != 0;
    if (stack_padding) {
        codegen_output_write(context->output, "    sub rsp, 8\n");
        context->stack_depth += 8;
    }

    emit_assembly_template(context, NULL, instruction);
    codegen_output_printf(
        context->output,
        ".section .text.%s,\"ax\",@progbits\n",
        function->declaration->function.name
    );

    if (stack_padding) {
        codegen_output_write(context->output, "    add rsp, 8\n");
        context->stack_depth -= 8;
    }

    for (unsigned long index = instruction->output_count; index > 0; index--) {
        codegen_output_printf(
            context->output,
            "    push %s\n",
            instruction->output_constraints[index - 1] + 1
        );
        context->stack_depth += 8;
    }
    for (unsigned long index = 0;
         index < instruction->output_count;
         index++)
    {
        const ir_value *value = &function->values[
            instruction->operand_values[index]
        ];
        unsigned long size = codegen_x86_64_context_type_size(
            context,
            value->type.name,
            value->type.pointer_depth
        );
        codegen_output_write(context->output, "    pop rcx\n");
        codegen_output_write(context->output, "    pop rax\n");
        context->stack_depth -= 16;
        codegen_output_printf(
            context->output,
            "    mov %s [rax], %s\n",
            codegen_x86_64_type_operand_size(
                value->type.name,
                value->type.pointer_depth
            ),
            assembly_low_register(size)
        );
    }

    while (saved_count > 0) {
        codegen_output_printf(
            context->output,
            "    pop %s\n",
            saved_registers[--saved_count]
        );
        context->stack_depth -= 8;
    }
}


/*
 * Generate an if statement.
 *
 * Example:
 *
 *     if (condition) {
 *         ...
 *     } else {
 *         ...
 *     }
 */
static void
emit_if(
    codegen_x86_64_context *context,
    ast *statement
)
{
    unsigned long else_label;
    unsigned long end_label;


    else_label =
        context->label_count++;

    end_label =
        context->label_count++;


    /*
     * ------------------------------------------------------------
     * if condition
     * ------------------------------------------------------------
     *
     * source:
     *
     *     if (condition)
     *
     * rax = condition
     */
    codegen_x86_64_expression(
        context,
        statement->if_statement.condition
    );


    /*
     * A lowC condition is true when it is non-zero.
     */
    codegen_output_write(
        context->output,
        "    cmp rax, 0\n"
    );


    if (statement->if_statement.else_body){

        /*
         * False -> else body.
         */
        codegen_output_printf(
            context->output,
            "    je .L%lu\n",
            else_label
        );

    } else {

        /*
         * False -> end of the if statement.
         */
        codegen_output_printf(
            context->output,
            "    je .L%lu\n",
            end_label
        );
    }


    /*
     * ------------------------------------------------------------
     * if body
     * ------------------------------------------------------------
     *
     * source:
     *
     *     if (condition) {
     *         ...
     *     }
     */
    codegen_x86_64_statement(
        context,
        statement->if_statement.then_body
    );


    if (statement->if_statement.else_body){

        /*
         * The then body has finished.
         *
         * Skip over the else body.
         */
        codegen_output_printf(
            context->output,
            "    jmp .L%lu\n"
            "\n"
            ".L%lu:\n",
            end_label,
            else_label
        );


        /*
         * --------------------------------------------------------
         * else body
         * --------------------------------------------------------
         *
         * source:
         *
         *     else {
         *         ...
         *     }
         */
        codegen_x86_64_statement(
            context,
            statement->if_statement.else_body
        );
    }


    /*
     * End of the if statement.
     */
    codegen_output_printf(
        context->output,
        "\n"
        ".L%lu:\n",
        end_label
    );
}


/*
 * Generate a for loop.
 *
 * Example:
 *
 *     for (init; condition; increment) {
 *         body;
 *     }
 */
static void
emit_for(
    codegen_x86_64_context *context,
    ast *statement
)
{
    unsigned long condition_label;
    unsigned long end_label;


    condition_label =
        context->label_count++;

    end_label =
        context->label_count++;


    /*
     * ------------------------------------------------------------
     * for initialization
     * ------------------------------------------------------------
     *
     * source:
     *
     *     for (init; condition; increment)
     */
    if (statement->for_statement.init){

        codegen_x86_64_statement(
            context,
            statement->for_statement.init
        );
    }


    /*
     * ------------------------------------------------------------
     * for condition label
     * ------------------------------------------------------------
     */
    codegen_output_printf(
        context->output,
        "\n"
        ".L%lu:\n",
        condition_label
    );


    /*
     * A missing condition means an infinite loop.
     *
     *     for (;;) {
     *         ...
     *     }
     */
    if (statement->for_statement.condition){

        /*
         * --------------------------------------------------------
         * for condition
         * --------------------------------------------------------
         *
         * source:
         *
         *     for (...; condition; ...)
         */
        codegen_x86_64_expression(
            context,
            statement->for_statement.condition
        );


        /*
         * Leave the loop when the condition is false.
         */
        codegen_output_write(
            context->output,
            "    cmp rax, 0\n"
        );

        codegen_output_printf(
            context->output,
            "    je .L%lu\n",
            end_label
        );
    }


    /*
     * ------------------------------------------------------------
     * for body
     * ------------------------------------------------------------
     *
     * source:
     *
     *     for (...) {
     *         body
     *     }
     */
    codegen_x86_64_statement(
        context,
        statement->for_statement.body
    );


    /*
     * ------------------------------------------------------------
     * for increment
     * ------------------------------------------------------------
     *
     * source:
     *
     *     for (...; ...; increment)
     * ------------------------------------------------------------
     */
    if (statement->for_statement.increment){

        codegen_x86_64_statement(
            context,
            statement->for_statement.increment
        );
    }


    /*
     * Go back to the condition.
     */
    codegen_output_printf(
        context->output,
        "    jmp .L%lu\n"
        "\n"
        ".L%lu:\n",
        condition_label,
        end_label
    );
}


/*
 * Generate one AST statement.
 */
void
codegen_x86_64_statement(
    codegen_x86_64_context *context,
    ast *statement
)
{
    unsigned long i;


    if (!context || !statement){
        return;
    }

    codegen_x86_64_emit_location(context, statement);

    switch (statement->type){

        /*
         * --------------------------------------------------------
         * Local variable declaration
         * --------------------------------------------------------
         *
         * Example:
         *
         *     s8 x;
         *
         * or:
         *
         *     s8 y = 42;
         */
        case AST_VARIABLE_DECLARATION:
        {
            codegen_x86_64_local *local;


            local =
                codegen_x86_64_find_local(
                    context,
                    statement->variable.name
                );


            /*
             * ----------------------------------------------------
             * local variable
             * ----------------------------------------------------
             */
            codegen_output_write(
                context->output,
                "    # ------------------------------------------------------------\n"
            );


            codegen_output_write(
                context->output,
                "    # local variable: "
            );

            codegen_output_write(
                context->output,
                statement->variable.name
            );

            codegen_output_write(
                context->output,
                "\n"
            );


            /*
             * Write the declared type.
             */
            codegen_output_write(
                context->output,
                "    # type: "
            );

            codegen_output_write(
                context->output,
                statement->variable.type
            );


            if (statement->variable.pointer_depth != 0){
                unsigned long pointer;


                for (
                    pointer = 0;
                    pointer < statement->variable.pointer_depth;
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


            /*
             * Write the stack location when the local was
             * successfully allocated.
             */
            if (local){

                codegen_output_printf(
                    context->output,
                    "    # stack location: [rbp%+ld]\n",
                    local->offset
                );
            }


            /*
             * Write the original source declaration.
             */
            codegen_output_write(
                context->output,
                "    # source: "
            );


            codegen_output_write(
                context->output,
                statement->variable.type
            );


            if (statement->variable.pointer_depth != 0){
                unsigned long pointer;


                for (
                    pointer = 0;
                    pointer < statement->variable.pointer_depth;
                    pointer++
                ){
                    codegen_output_write(
                        context->output,
                        " *"
                    );
                }
            }


            codegen_output_printf(
                context->output,
                " %s",
                statement->variable.name
            );


            if (statement->variable.value){

                codegen_output_write(
                    context->output,
                    " = "
                );

                emit_expression_source(
                    context,
                    statement->variable.value
                );
            }


            codegen_output_write(
                context->output,
                ";\n"
            );


            /*
             * ----------------------------------------------------
             * End of local-variable description.
             * ----------------------------------------------------
             */
            codegen_output_write(
                context->output,
                "    # ------------------------------------------------------------\n"
            );


            /*
             * If the declaration has an initializer,
             * generate it and store it into the local.
             */
            if (statement->variable.value && local){

                int is_aggregate =
                    local->array_count != 0 ||
                    (
                        local->pointer_depth == 0 &&
                        codegen_x86_64_type_size(
                            local->type,
                            0
                        ) == 0
                    );

                if (
                    is_aggregate &&
                    codegen_x86_64_zero_initializer(
                        statement->variable.value
                    )
                ) {
                    codegen_output_printf(
                        context->output,
                        "    lea rdi, [rbp%+ld]\n"
                        "    mov rcx, %lu\n"
                        "    xor eax, eax\n"
                        "    rep stosb\n",
                        local->offset,
                        local->size
                    );
                } else {
                    /*
                     * Initialize scalar and pointer locals from their value.
                     */
                    codegen_x86_64_expression(
                        context,
                        statement->variable.value
                    );

                    emit_local_store(
                        context,
                        local
                    );
                }


                codegen_output_write(
                    context->output,
                    "\n"
                );
            }


            return;
        }


        /*
         * --------------------------------------------------------
         * Assignment
         * --------------------------------------------------------
         *
         * Assignments normally arrive here wrapped inside an
         * AST_EXPRESSION_STATEMENT.
         */
        case AST_ASSIGNMENT:

            emit_assignment(
                context,
                statement
            );

            return;

        case AST_MULTI_ASSIGNMENT:
            emit_multi_assignment(context, statement);
            return;


        /*
         * --------------------------------------------------------
         * Expression statement
         * --------------------------------------------------------
         *
         * Example:
         *
         *     foo();
         *
         * or:
         *
         *     x = y;
         *
         * The parser represents both as an expression statement.
         */
        case AST_EXPRESSION_STATEMENT:

            if (
                statement->expression_statement.expression &&
                statement->expression_statement.expression->type ==
                    AST_ASSIGNMENT
            ){
                ast *assignment =
                    statement->expression_statement.expression;


                /*
                 * ----------------------------------------------------
                 * assignment
                 *
                 * source:
                 *
                 *     x = y;
                 * ----------------------------------------------------
                 */
                codegen_output_write(
                    context->output,
                    "    # ------------------------------------------------------------\n"
                    "    # assignment\n"
                    "    # source: "
                );


                /*
                 * Print the assignment from its AST.
                 */
                if (
                    assignment->assignment.left &&
                    assignment->assignment.right
                ){
                    emit_expression_source(
                        context,
                        assignment->assignment.left
                    );

                    codegen_output_write(
                        context->output,
                        " = "
                    );

                    emit_expression_source(
                        context,
                        assignment->assignment.right
                    );

                    codegen_output_write(
                        context->output,
                        ";\n"
                    );

                } else {

                    codegen_output_write(
                        context->output,
                        "assignment;\n"
                    );
                }


                codegen_output_write(
                    context->output,
                    "    # ------------------------------------------------------------\n"
                );


                emit_assignment(
                    context,
                    assignment
                );


                codegen_output_write(
                    context->output,
                    "\n"
                );

            } else {

                codegen_x86_64_expression(
                    context,
                    statement->expression_statement.expression
                );
            }

            return;


        /*
         * --------------------------------------------------------
         * if / else
         * --------------------------------------------------------
         */
        case AST_IF:

            emit_if(
                context,
                statement
            );

            return;


        /*
         * --------------------------------------------------------
         * for loop
         * --------------------------------------------------------
         */
        case AST_FOR:

            emit_for(
                context,
                statement
            );

            return;


        /*
         * --------------------------------------------------------
         * return
         * --------------------------------------------------------
         *
         * Example:
         *
         *     return 128;
         *
         * The expression result is left in rax.
         */
        case AST_RETURN:
            return;

        case AST_ASM:
            emit_assembly(context, statement);
            return;


        /*
         * --------------------------------------------------------
         * Block
         * --------------------------------------------------------
         *
         * {
         *     statement;
         *     statement;
         * }
         */
        case AST_BLOCK:

            for (
                i = 0;
                i < statement->block.statements.count;
                i++
            ){
                codegen_x86_64_statement(
                    context,
                    statement->block.statements.values[i]
                );
            }

            return;


        default:

            /*
             * This should eventually be reported through
             * generic codegen error handling.
             */
            return;
    }
}

static void
emit_ir_declaration(
    codegen_x86_64_context *context,
    const ir_function *function,
    const ir_instruction *instruction
)
{
    codegen_x86_64_local *local = codegen_x86_64_find_local(
        context,
        instruction->symbol_name
    );
    const ast *initializer = instruction->result_value == ULONG_MAX
        ? NULL
        : function->values[instruction->result_value].source;

    codegen_x86_64_emit_location(context, instruction->source);
    codegen_output_printf(
        context->output,
        "    # local variable: %s\n"
        "    # type: %s",
        instruction->symbol_name,
        instruction->type.name
    );
    for (unsigned long index = 0;
         index < instruction->type.pointer_depth;
         index++)
    {
        codegen_output_write(context->output, " *");
    }
    codegen_output_write(context->output, "\n");

    if (local) {
        codegen_output_printf(
            context->output,
            "    # stack location: [rbp%+ld]\n",
            local->offset
        );
    }

    codegen_output_write(context->output, "    # source: ");
    codegen_output_printf(
        context->output,
        "%s",
        instruction->type.name
    );
    for (unsigned long index = 0;
         index < instruction->type.pointer_depth;
         index++)
    {
        codegen_output_write(context->output, " *");
    }
    codegen_output_printf(
        context->output,
        " %s",
        instruction->symbol_name
    );
    if (initializer) {
        codegen_output_write(context->output, " = ");
        emit_expression_source(context, (ast *)initializer);
    }
    codegen_output_write(context->output, ";\n");

    if (!initializer || !local) {
        return;
    }

    if (
        (local->array_count != 0 ||
         (
             local->pointer_depth == 0 &&
             codegen_x86_64_type_size(local->type, 0) == 0
         )) &&
        codegen_x86_64_zero_initializer((ast *)initializer)
    ) {
        codegen_output_printf(
            context->output,
            "    lea rdi, [rbp%+ld]\n"
            "    mov rcx, %lu\n"
            "    xor eax, eax\n"
            "    rep stosb\n",
            local->offset,
            local->size
        );
    } else {
        codegen_x86_64_ir_expression_id(
            context,
            instruction->result_value
        );
        emit_local_store(context, local);
    }
}

static void
emit_ir_assignment(
    codegen_x86_64_context *context,
    const ir_function *function,
    const ir_instruction *instruction
)
{
    const ir_value *left = &function->values[instruction->left_value];
    codegen_x86_64_local *local;
    const char *operand_size;
    const char *register_name;

    codegen_x86_64_lvalue_address(context, (ast *)left->source);
    codegen_output_write(context->output, "    push rax\n");
    context->stack_depth += 8;

    codegen_x86_64_ir_expression_id(
        context,
        instruction->right_value
    );

    codegen_output_write(context->output, "    pop rcx\n");
    context->stack_depth -= 8;

    if (left->opcode == IR_VALUE_LOAD) {
        local = codegen_x86_64_find_local(context, left->name);
        if (local) {
            emit_local_store(context, local);
            return;
        }
    }

    operand_size = codegen_x86_64_type_operand_size(
        instruction->type.name,
        instruction->type.pointer_depth
    );
    register_name = codegen_x86_64_type_register(
        instruction->type.name,
        instruction->type.pointer_depth
    );
    if (operand_size && register_name) {
        codegen_output_printf(
            context->output,
            "    mov %s [rcx], %s\n",
            operand_size,
            register_name
        );
    } else {
        codegen_output_write(
            context->output,
            "    mov QWORD PTR [rcx], rax\n"
        );
    }
}

void
codegen_x86_64_ir_statement(
    codegen_x86_64_context *context,
    const ir_function *function,
    const ir_instruction *instruction
)
{
    if (!context || !function || !instruction) {
        return;
    }

    switch (instruction->opcode) {
        case IR_INSTRUCTION_EXPRESSION:
            if (instruction->result_value != ULONG_MAX) {
                codegen_x86_64_ir_expression_id(
                    context,
                    instruction->result_value
                );
            }
            return;

        case IR_INSTRUCTION_ASSIGNMENT:
            emit_ir_assignment(context, function, instruction);
            return;

        case IR_INSTRUCTION_DECLARATION:
            emit_ir_declaration(context, function, instruction);
            return;

        case IR_INSTRUCTION_ASSEMBLY:
            emit_ir_assembly(context, function, instruction);
            return;

        case IR_INSTRUCTION_MULTI_ASSIGNMENT:
            emit_ir_multi_assignment(context, function, instruction);
            return;

        case IR_INSTRUCTION_RETURN:
            emit_ir_return(context, function, instruction);
            return;
    }
}
