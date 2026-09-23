#include "context.h"

#include <stdlib.h>
#include <string.h>


/*
 * Duplicate a string.
 */
static char *
codegen_x86_64_strdup(
    const char *text
)
{
    size_t length;
    char *copy;


    if (!text){
        return NULL;
    }


    length =
        strlen(
            text
        );


    copy =
        malloc(
            length + 1
        );


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


/*
 * Round value up to alignment.
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
 * Initialize the x86-64 function context.
 */
void
codegen_x86_64_context_init(
    codegen_x86_64_context *context,
    codegen_output *output
)
{
    if (!context){
        return;
    }


    memset(
        context,
        0,
        sizeof(*context)
    );


    /*
     * The assembly buffer belongs to generic codegen.
     *
     * This backend only keeps a pointer to it.
     */
    context->output = output;
}


/*
 * Free all state belonging to the context.
 */
void
codegen_x86_64_context_free(
    codegen_x86_64_context *context
)
{
    unsigned long i;


    if (!context){
        return;
    }


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


    memset(
        context,
        0,
        sizeof(*context)
    );
}


/*
 * Add a local variable.
 *
 * Stack layout grows downward from rbp.
 *
 * The local also keeps its source-level type information
 * and array count.
 */
codegen_x86_64_local *
codegen_x86_64_add_local(
    codegen_x86_64_context *context,
    const char *name,
    const char *type,
    unsigned long pointer_depth,
    unsigned long array_count,
    unsigned long size,
    unsigned long alignment
)
{
    codegen_x86_64_local *new_values;
    codegen_x86_64_local *local;
    unsigned long offset;


    if (
        !context ||
        !name ||
        !type
    ){
        return NULL;
    }


    if (size == 0){
        size = 1;
    }


    if (alignment == 0){
        alignment = 1;
    }


    /*
     * Align the next local correctly.
     */
    context->stack_size =
        align_up(
            context->stack_size,
            alignment
        );


    /*
     * Reserve space for this local.
     *
     * For an array, size has already been calculated as:
     *
     *     element_size * array_count
     */
    context->stack_size += size;


    /*
     * The local lives below rbp.
     */
    offset =
        context->stack_size;


    /*
     * Grow the local array.
     */
    new_values =
        realloc(
            context->locals.values,
            sizeof(codegen_x86_64_local) *
            (context->locals.count + 1)
        );


    if (!new_values){
        /*
         * The stack size has already been changed.
         *
         * Roll it back because the local was not added.
         */
        context->stack_size -= size;

        return NULL;
    }


    context->locals.values =
        new_values;


    local =
        &context->locals.values[
            context->locals.count
        ];


    memset(
        local,
        0,
        sizeof(*local)
    );


    local->name =
        codegen_x86_64_strdup(
            name
        );


    if (!local->name){
        return NULL;
    }


    local->type =
        codegen_x86_64_strdup(
            type
        );


    if (!local->type){
        free(
            local->name
        );

        local->name = NULL;

        return NULL;
    }


    local->pointer_depth =
        pointer_depth;

    local->array_count =
        array_count;

    local->size =
        size;

    local->alignment =
        alignment;

    local->offset =
        -(long)offset;


    context->locals.count++;


    return local;
}


/*
 * Find a local variable by name.
 *
 * Search backwards so the newest declaration wins.
 */
codegen_x86_64_local *
codegen_x86_64_find_local(
    codegen_x86_64_context *context,
    const char *name
)
{
    long i;


    if (
        !context ||
        !name
    ){
        return NULL;
    }


    for (
        i = (long)context->locals.count - 1;
        i >= 0;
        i--
    ){
        if (
            strcmp(
                context->locals.values[i].name,
                name
            ) == 0
        ){
            return
                &context->locals.values[i];
        }
    }


    return NULL;
}
