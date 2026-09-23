#include "public.h"
#include "internal.h"

#include "target/x86_64/context.h"
#include "target/x86_64/function.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char *
codegen_strdup(
    const char *text
)
{
    size_t length;
    char *copy;

    if (!text){
        return NULL;
    }

    length = strlen(text);

    copy = malloc(length + 1);

    if (!copy){
        return NULL;
    }

    memcpy(
        copy,
        text,
        length + 1
    );

    return copy;
}


static void
codegen_set_error(
    codegen_context *context,
    const char *message
)
{
    context->result->error = 1;

    free(
        context->result->error_text
    );

    context->result->error_text =
        codegen_strdup(message);
}


void
codegen_fail(
    codegen_context *context,
    const char *message
)
{
    codegen_set_error(
        context,
        message
    );
}


char *
codegen_make_label(
    codegen_context *context,
    const char *prefix
)
{
    char buffer[128];

    snprintf(
        buffer,
        sizeof(buffer),
        ".L_%s_%lu",
        prefix,
        context->label_counter++
    );

    return codegen_strdup(buffer);
}


int
codegen_generate_node(
    codegen_context *context,
    ast *node
)
{
    unsigned long i;

    if (!node){
        return 0;
    }

    switch (node->type){

        case AST_PROGRAM:

            /*
             * Generate every function in the program.
             */
            for (
                i = 0;
                i < node->program.functions.count;
                i++
            ){
                if (
                    codegen_generate_node(
                        context,
                        node->program.functions.values[i]
                    )
                ){
                    return 1;
                }
            }

            return 0;


        case AST_FUNCTION:
        {
            codegen_x86_64_context x86_context;

            /*
             * The generic codegen output buffer remains owned
             * by this context. The target backend only receives
             * a pointer to it.
             */
            codegen_x86_64_context_init(
                &x86_context,
                &context->output
            );

            codegen_x86_64_function(
                &x86_context,
                node
            );

            codegen_x86_64_context_free(
                &x86_context
            );

            return 0;
        }


        default:

            codegen_fail(
                context,
                "unsupported top-level AST node"
            );

            return 1;
    }
}


int
codegen_generate_program(
    codegen_context *context,
    ast *tree
)
{
    if (!context || !tree){
        return 1;
    }

    return codegen_generate_node(
        context,
        tree
    );
}


codegen_result
codegen(
    ast *tree,
    flag_handler_options *options
)
{
    codegen_result result = {0};
    codegen_context context = {0};

    if (!tree){
        result.error = 1;

        result.error_text =
            codegen_strdup(
                "cannot generate code from null AST"
            );

        return result;
    }

    if (!options){
        result.error = 1;

        result.error_text =
            codegen_strdup(
                "missing compiler flags"
            );

        return result;
    }

    /*
     * x86-64 is currently the only backend.
     *
     * Keep the target check here so adding another target
     * later has one obvious place.
     */
    if (
        options->target !=
        FLAG_TARGET_X86_64
    ){
        result.error = 1;

        result.error_text =
            codegen_strdup(
                "unsupported code generation target"
            );

        return result;
    }

    context.result = &result;

    /*
     * Generic codegen owns the assembly buffer.
     */
    codegen_output_init(
        &context.output
    );

    if (
        codegen_generate_program(
            &context,
            tree
        )
    ){
        if (!result.error){
            result.error = 1;

            result.error_text =
                codegen_strdup(
                    "code generation failed"
                );
        }
    }

    /*
     * Transfer ownership of the generated assembly
     * to the result.
     */
    if (!result.error){

        result.assembly =
            context.output.data;

        context.output.data = NULL;
        context.output.size = 0;
        context.output.capacity = 0;
    }

    codegen_output_free(
        &context.output
    );

    return result;
}


void
codegen_handle_error(
    codegen_result result
)
{
    if (!result.error){
        return;
    }

    if (result.file){

        fprintf(
            stderr,
            "%s:%lu:%lu: ",
            result.file,
            result.line,
            result.column
        );
    }

    fprintf(
        stderr,
        "%s\n",
        result.error_text
            ? result.error_text
            : "code generation failed"
    );
}


void
codegen_free(
    codegen_result result
)
{
    free(result.error_text);
    free(result.file);
    free(result.source_line);
    free(result.correct);
    free(result.assembly);
}
