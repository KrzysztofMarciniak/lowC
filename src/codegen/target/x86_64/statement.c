#include "statement.h"
#include "expression.h"
#include "type.h"

#include "../../output.h"

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
    ast *assembly
)
{
    const char *text = assembly->assembly.template;
    unsigned long output_count = assembly->assembly.outputs.count;
    unsigned long input_count = assembly->assembly.inputs.count;
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
                        assembly_output_register(
                            assembly->assembly.outputs.values[operand_index]
                        )
                    );
                } else {
                    operand_index -= output_count;
                    if (operand_index < input_count) {
                        if (line_start)
                            codegen_output_write(context->output, "    ");
                        codegen_output_write(
                            context->output,
                            assembly_input_register(
                                assembly->assembly.inputs.values[operand_index]
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

    emit_assembly_template(context, assembly);

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

            codegen_output_write(
                context->output,
                "    # ------------------------------------------------------------\n"
                "    # return\n"
                "    # source: return"
            );


            if (statement->return_statement.value){

                codegen_output_write(
                    context->output,
                    " '"
                );


                emit_expression_source(
                    context,
                    statement->return_statement.value
                );


                codegen_output_write(
                    context->output,
                    "'"
                );
            }


            codegen_output_write(
                context->output,
                ";\n"
                "    # ------------------------------------------------------------\n"
            );


            /*
             * Generate the return value.
             *
             * The result remains in rax according to the
             * System V AMD64 calling convention.
             */
            if (statement->return_statement.value){

                codegen_x86_64_expression(
                    context,
                    statement->return_statement.value
                );
            }


            codegen_output_printf(
                context->output,
                "\n"
                "    # return\n"
                "    # transfer control to function epilogue\n"
                "    jmp .L%lu\n",
                context->epilogue_label
            );

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
