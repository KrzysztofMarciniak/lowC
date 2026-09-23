#include "function.h"
#include "statement.h"
#include "type.h"

#include "../../output.h"

#include <stdlib.h>


/*
 * System V AMD64 integer/pointer argument registers.
 *
 * The first six integer or pointer arguments are passed
 * in these registers.
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


/*
 * Round a value up to the requested alignment.
 */
static unsigned long
align_up(
    unsigned long value,
    unsigned long alignment
)
{
    unsigned long remainder;


    if (alignment == 0){
        return value;
    }


    remainder =
        value % alignment;


    if (remainder == 0){
        return value;
    }


    return
        value +
        alignment -
        remainder;
}


/*
 * Write a lowC type to the assembly output.
 *
 * Examples:
 *
 *     s64
 *     u32
 *     s64 *
 *     u8 **
 */
static void
emit_type(
    codegen_output *output,
    const char *type,
    unsigned long pointer_depth
)
{
    unsigned long i;


    if (!type){
        return;
    }


    codegen_output_write(
        output,
        type
    );


    for (
        i = 0;
        i < pointer_depth;
        i++
    ){
        codegen_output_write(
            output,
            " *"
        );
    }
}


/*
 * Write a complete function signature to the assembly
 * comments.
 *
 * Example:
 *
 *     s64 entrypoint(void)
 *
 * Example:
 *
 *     s64 add(s64 left, s64 right)
 */
static void
emit_function_signature(
    codegen_x86_64_context *context,
    ast *function
)
{
    unsigned long i;


    /*
     * Return type.
     */
    emit_type(
        context->output,
        function->function.return_type,
        function->function.return_pointer_depth
    );


    codegen_output_printf(
        context->output,
        " %s(",
        function->function.name
    );


    /*
     * A function with no parameters is represented as
     * void in the assembly source comment.
     *
     * This is only a source-mapping comment. It does not
     * change the lowC syntax.
     */
    if (function->function.parameters.count == 0){

        codegen_output_write(
            context->output,
            "void"
        );

    } else {

        for (
            i = 0;
            i < function->function.parameters.count;
            i++
        ){
            ast *parameter =
                function->function.parameters.values[i];


            if (i != 0){

                codegen_output_write(
                    context->output,
                    ", "
                );
            }


            emit_type(
                context->output,
                parameter->parameter.type,
                parameter->parameter.pointer_depth
            );


            codegen_output_printf(
                context->output,
                " %s",
                parameter->parameter.name
            );
        }
    }


    codegen_output_write(
        context->output,
        ")\n"
    );
}


/*
 * Collect all local variables from a statement tree.
 *
 * The actual stack frame must be known before the function
 * prologue is emitted, so locals are collected first.
 */
static void
collect_locals(
    codegen_x86_64_context *context,
    ast *statement
)
{
    unsigned long i;


    if (!statement){
        return;
    }


    switch (statement->type){

        case AST_VARIABLE_DECLARATION:
        {
            unsigned long size;
            unsigned long alignment;
            unsigned long array_count;


            array_count = 0;


            size =
                codegen_x86_64_type_size(
                    statement->variable.type,
                    statement->variable.pointer_depth
                );


            alignment =
                codegen_x86_64_type_alignment(
                    statement->variable.type,
                    statement->variable.pointer_depth
                );


            if (statement->variable.array_length != NULL)
            {
                array_count =
                    strtoul(
                        statement->variable.array_length->integer.value,
                        NULL,
                        10
                    );


                size *= array_count;
            }


            codegen_x86_64_add_local(
                context,
                statement->variable.name,
                statement->variable.type,
                statement->variable.pointer_depth,
                array_count,
                size,
                alignment
            );


            break;
        }


        case AST_BLOCK:

            for (
                i = 0;
                i < statement->block.statements.count;
                i++
            ){
                collect_locals(
                    context,
                    statement->block.statements.values[i]
                );
            }

            break;


        case AST_IF:

            collect_locals(
                context,
                statement->if_statement.then_body
            );


            collect_locals(
                context,
                statement->if_statement.else_body
            );

            break;


        case AST_FOR:

            collect_locals(
                context,
                statement->for_statement.init
            );


            collect_locals(
                context,
                statement->for_statement.body
            );


            collect_locals(
                context,
                statement->for_statement.increment
            );

            break;


        default:
            break;
    }
}


/*
 * Emit function parameters.
 *
 * Parameters arrive in the System V AMD64 argument
 * registers and are immediately copied into their
 * local stack slots.
 */
static void
emit_parameters(
    codegen_x86_64_context *context,
    ast *function
)
{
    unsigned long i;

    codegen_x86_64_local *local;

    const char *register_name;
    const char *operand_size;


    /*
     * Keep the current backend limited to the six
     * register-passed integer/pointer arguments.
     */
    if (
        function->function.parameters.count >
        6
    ){
        return;
    }


    for (
        i = 0;
        i < function->function.parameters.count;
        i++
    ){
        ast *parameter =
            function->function.parameters.values[i];


        local =
            codegen_x86_64_find_local(
                context,
                parameter->parameter.name
            );


        if (!local){
            return;
        }


        register_name =
            argument_registers[i];


        operand_size =
            codegen_x86_64_type_operand_size(
                parameter->parameter.type,
                parameter->parameter.pointer_depth
            );


        if (!operand_size){
            return;
        }


        /*
         * --------------------------------------------------------
         * Function parameter
         * --------------------------------------------------------
         *
         * source:
         *
         *     <type> <name>
         *
         * The parameter arrives in an ABI register and is copied
         * into its local stack slot.
         */
        codegen_output_printf(
            context->output,
            "    # parameter %s\n",
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
            "    # source: %s %s\n",
            local->type,
            local->name
        );


        codegen_output_printf(
            context->output,
            "    mov %s [rbp%+ld], %s\n\n",
            operand_size,
            local->offset,
            register_name
        );
    }
}


/*
 * Reset all function-local state.
 *
 * The x86-64 context can then be reused for the next
 * function in the source file.
 */
static void
reset_function_locals(
    codegen_x86_64_context *context
)
{
    unsigned long i;


    for (
        i = 0;
        i < context->locals.count;
        i++
    ){
        free(
            context->locals.values[i].name
        );


        free(
            context->locals.values[i].type
        );
    }


    free(
        context->locals.values
    );


    context->locals.values = NULL;
    context->locals.count = 0;

    context->stack_size = 0;
    context->stack_depth = 0;
    context->label_count = 0;
}


/*
 * Generate one complete x86-64 function.
 */
void
codegen_x86_64_function(
    codegen_x86_64_context *context,
    ast *function
)
{
    unsigned long i;
    unsigned long frame_size;


    if (
        !context ||
        !function
    ){
        return;
    }


    /*
     * Make sure this context starts clean for the
     * current function.
     */
    reset_function_locals(
        context
    );


    /*
     * ------------------------------------------------------------
     * Collect function parameters
     * ------------------------------------------------------------
     *
     * Parameters are represented as locals so that the rest of
     * code generation can access them through their normal
     * rbp-relative stack locations.
     */
    for (
        i = 0;
        i < function->function.parameters.count;
        i++
    ){
        ast *parameter =
            function->function.parameters.values[i];


        unsigned long size =
            codegen_x86_64_type_size(
                parameter->parameter.type,
                parameter->parameter.pointer_depth
            );


        unsigned long alignment =
            codegen_x86_64_type_alignment(
                parameter->parameter.type,
                parameter->parameter.pointer_depth
            );


        codegen_x86_64_add_local(
            context,
            parameter->parameter.name,
            parameter->parameter.type,
            parameter->parameter.pointer_depth,
            0,
            size,
            alignment
        );
    }


    /*
     * ------------------------------------------------------------
     * Collect local variables
     * ------------------------------------------------------------
     *
     * The stack frame must be completely known before the
     * function prologue is emitted.
     */
    collect_locals(
        context,
        function->function.body
    );


    /*
     * Round the complete frame up to 16 bytes.
     */
    frame_size =
        align_up(
            context->stack_size,
            16
        );


    /*
     * ------------------------------------------------------------
     * Function declaration
     * ------------------------------------------------------------
     */
    codegen_output_write(
    context->output,
    ".intel_syntax noprefix\n\n"
);
    codegen_output_printf(
        context->output,
        ".globl %s\n",
        function->function.name
    );


    codegen_output_printf(
        context->output,
        ".type %s, @function\n\n",
        function->function.name
    );


    /*
     * ------------------------------------------------------------
     * Function label
     * ------------------------------------------------------------
     */
    codegen_output_printf(
        context->output,
        "%s:\n",
        function->function.name
    );


    /*
     * ------------------------------------------------------------
     * Function prologue
     *
     * source:
     *
     *     <return type> <name>(<parameters>) {
     * ------------------------------------------------------------
     */
    codegen_output_write(
        context->output,
        "    # ------------------------------------------------------------\n"
        "    # function prologue\n"
        "    # source: "
    );


    emit_function_signature(
        context,
        function
    );


    codegen_output_write(
        context->output,
        "    # ------------------------------------------------------------\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
    );


    /*
     * Reserve the stack space required by parameters
     * and local variables.
     */
    if (frame_size != 0){

        codegen_output_printf(
            context->output,
            "    sub rsp, %lu\n",
            frame_size
        );
    }


    codegen_output_write(
        context->output,
        "\n"
    );


    /*
     * ------------------------------------------------------------
     * Function parameters
     * ------------------------------------------------------------
     *
     * Move incoming ABI registers into their local stack slots.
     */
    emit_parameters(
        context,
        function
    );


    /*
     * ------------------------------------------------------------
     * Function body
     * ------------------------------------------------------------
     */
    if (function->function.body){

        codegen_x86_64_statement(
            context,
            function->function.body
        );
    }


    /*
     * ------------------------------------------------------------
     * Function epilogue
     * ------------------------------------------------------------
     */
    codegen_output_write(
        context->output,
        "\n"
        ".L_function_epilogue:\n"
        "    # ------------------------------------------------------------\n"
        "    # function epilogue\n"
        "    # source: end of function\n"
        "    # ------------------------------------------------------------\n"
        "    leave\n"
        "    ret\n"
        "\n"
    );
}

