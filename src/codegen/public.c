#include "public.h"
#include "internal.h"

#include "target/x86_64/context.h"
#include "target/x86_64/function.h"

#include <errno.h>
#include <limits.h>
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

static int
codegen_constant_integer(
    ast *expression,
    long long *value
)
{
    char *end;

    if (!expression || !value) {
        return 0;
    }

    if (expression->type == AST_UNARY) {
        if (
            expression->unary.operator != AST_UNARY_MINUS &&
            expression->unary.operator != AST_UNARY_PLUS
        ) {
            return 0;
        }
        if (!codegen_constant_integer(
                expression->unary.operand,
                value
            )) {
            return 0;
        }
        if (expression->unary.operator == AST_UNARY_MINUS) {
            if (*value == LLONG_MIN) {
                return 0;
            }
            *value = -*value;
        }
        return 1;
    }

    if (expression->type != AST_INTEGER) {
        return 0;
    }

    errno = 0;
    *value = strtoll(expression->integer.value, &end, 10);
    return errno == 0 && *end == '\0';
}

static const char *
codegen_global_data_directive(
    unsigned long size
)
{
    switch (size) {
        case 1:
            return ".byte";
        case 2:
            return ".word";
        case 4:
            return ".long";
        case 8:
            return ".quad";
        default:
            return NULL;
    }
}

static int
codegen_emit_global(
    codegen_context *context,
    ast *global
)
{
    codegen_x86_64_context layout;
    unsigned long size;
    unsigned long alignment;
    const char *directive;
    long long initializer;

    codegen_x86_64_context_init(&layout, &context->output);
    layout.structs = context->structs;

    size = codegen_x86_64_context_type_size(
        &layout,
        global->variable.type,
        global->variable.pointer_depth
    );
    alignment = codegen_x86_64_context_type_alignment(
        &layout,
        global->variable.type,
        global->variable.pointer_depth
    );

    if (global->variable.array_length) {
        size *= strtoul(
            global->variable.array_length->integer.value,
            NULL,
            10
        );
    }

    if (size == 0) {
        codegen_fail(context, "cannot determine global variable size");
        return 1;
    }

    if (global->variable.value) {
        directive = codegen_global_data_directive(size);
        if (
            !directive ||
            global->variable.array_length ||
            !codegen_constant_integer(
                global->variable.value,
                &initializer
            )
        ) {
            codegen_fail(
                context,
                "global initializer must be a scalar integer constant"
            );
            return 1;
        }

        codegen_output_printf(
            &context->output,
            ".data\n.align %lu\n.globl %s\n.type %s, @object\n.size %s, %lu\n%s:\n    %s %lld\n",
            alignment,
            global->variable.name,
            global->variable.name,
            global->variable.name,
            size,
            global->variable.name,
            directive,
            initializer
        );
    } else {
        codegen_output_printf(
            &context->output,
            ".bss\n.align %lu\n.globl %s\n.type %s, @object\n.size %s, %lu\n%s:\n    .zero %lu\n",
            alignment,
            global->variable.name,
            global->variable.name,
            global->variable.name,
            size,
            global->variable.name,
            size
        );
    }

    codegen_x86_64_context_free(&layout);
    return 0;
}

static ast *
codegen_find_global(
    ast *program,
    const char *name
)
{
    unsigned long i;

    for (i = 0; i < program->program.globals.count; i++) {
        ast *global = program->program.globals.values[i];

        if (strcmp(global->variable.name, name) == 0) {
            return global;
        }
    }

    return NULL;
}

static int
codegen_emit_process_args_bridge(
    codegen_context *context,
    ast *program
)
{
    ast *argc_global;
    ast *argv_global;

    argc_global = codegen_find_global(
        program,
        "__lowc_args_argc"
    );
    argv_global = codegen_find_global(
        program,
        "__lowc_args_argv"
    );

    if (!argc_global && !argv_global) {
        return 0;
    }

    if (
        !argc_global ||
        !argv_global ||
        strcmp(argc_global->variable.type, "u8") != 0 ||
        argc_global->variable.pointer_depth != 0 ||
        argc_global->variable.array_length != NULL ||
        argc_global->variable.value != NULL ||
        strcmp(argv_global->variable.type, "u1") != 0 ||
        argv_global->variable.pointer_depth != 2 ||
        argv_global->variable.array_length != NULL ||
        argv_global->variable.value != NULL
    ) {
        codegen_fail(
            context,
            "invalid declarations for command-line argument runtime state"
        );
        return 1;
    }

    codegen_output_write(
        &context->output,
        "    mov edi, edi\n"
        "    mov [rip+__lowc_args_argc], rdi\n"
        "    mov [rip+__lowc_args_argv], rsi\n"
    );

    return 0;
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

            for (
                i = 0;
                i < node->program.globals.count;
                i++
            ) {
                if (codegen_emit_global(
                        context,
                        node->program.globals.values[i]
                    )) {
                    return 1;
                }
            }

            codegen_output_write(&context->output, ".text\n");

            /*
             * Generate every function in the program.
             */
            for (
                i = 0;
                i < node->program.functions.count;
                i++
            ){
                ast *function = node->program.functions.values[i];

                if (function->function.is_foreign)
                    continue;

                if (
                    codegen_generate_node(
                        context,
                        function
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
            x86_context.optimize = context->optimize;
            x86_context.label_count = context->label_counter;
            x86_context.structs = context->structs;
            x86_context.globals = context->globals;
            x86_context.functions = context->functions;

            codegen_x86_64_function(
                &x86_context,
                node
            );
            context->label_counter = x86_context.label_count;

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
    ast *entrypoint = NULL;

    if (!context || !tree){
        return 1;
    }

    if (tree->type == AST_PROGRAM) {
        context->structs = &tree->program.structs;
        context->globals = &tree->program.globals;
        context->functions = &tree->program.functions;
    }

    if (codegen_generate_node(context, tree)) {
        return 1;
    }

    if (!context->executable) {
        return 0;
    }

    if (tree->type != AST_PROGRAM) {
        codegen_fail(context, "executable output requires a program");
        return 1;
    }

    for (unsigned long i = 0; i < tree->program.functions.count; i++) {
        ast *function = tree->program.functions.values[i];

        if (strcmp(function->function.name, "entrypoint") == 0) {
            if (function->function.is_foreign)
                continue;

            if (entrypoint != NULL) {
                codegen_fail(context, "program must define exactly one entrypoint");
                return 1;
            }
            entrypoint = function;
        }

        if (strcmp(function->function.name, "main") == 0) {
            codegen_fail(context, "lowC programs use entrypoint, not main");
            return 1;
        }
    }

    if (entrypoint == NULL) {
        codegen_fail(context, "program must define an entrypoint function");
        return 1;
    }

    if (entrypoint->function.parameters.count != 0) {
        codegen_fail(context, "entrypoint must not take parameters");
        return 1;
    }

    if (
        strcmp(entrypoint->function.return_type, "void") != 0 ||
        entrypoint->function.return_pointer_depth != 0
    ) {
        codegen_fail(context, "entrypoint must return void");
        return 1;
    }

    codegen_output_write(
        &context->output,
        ".globl main\n"
        ".type main, @function\n"
        "main:\n"
        "    sub rsp, 8\n"
    );

    if (codegen_emit_process_args_bridge(context, tree)) {
        return 1;
    }

    codegen_output_write(
        &context->output,
        "    call entrypoint\n"
        "    add rsp, 8\n"
    );

    codegen_output_write(&context->output, "    xor eax, eax\n");

    codegen_output_write(
        &context->output,
        "    ret\n"
        ".size main, .-main\n"
    );

    return 0;
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
    context.optimize = options->optimize;
    context.executable = options->executable && !options->asm_output;

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
