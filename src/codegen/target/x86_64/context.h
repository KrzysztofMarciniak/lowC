#ifndef LOWC_CODEGEN_TARGET_X86_64_CONTEXT_H
#define LOWC_CODEGEN_TARGET_X86_64_CONTEXT_H

#include "../../output.h"
#include "../../../parser/public.h"
#include "../../../ir/public.h"


/*
 * One local variable inside an x86-64 function.
 *
 * offset is relative to rbp.
 *
 * Example:
 *
 *     offset = -8
 *
 * means:
 *
 *     [rbp-8]
 *
 * The source-level type is kept here so the backend knows
 * exactly how this local must be loaded and stored.
 */
typedef struct codegen_x86_64_local
{
    /*
     * Source-level variable name.
     *
     * Example:
     *
     *     sum
     */
    char *name;


    /*
     * Source-level type.
     *
     * Example:
     *
     *     s8
     */
    char *type;


    /*
     * Number of pointer indirections.
     *
     * Example:
     *
     *     s8       -> 0
     *     s8 *     -> 1
     *     s8 **    -> 2
     */
    unsigned long pointer_depth;



     unsigned long array_count;

    /*
     * Size of the value stored by this local.
     */
    unsigned long size;


    /*
     * Required alignment of the local.
     */
    unsigned long alignment;


    /*
     * Stack offset relative to rbp.
     *
     * Example:
     *
     *     -8
     *
     * means:
     *
     *     [rbp-8]
     */
    long offset;

} codegen_x86_64_local;


/*
 * List of local variables.
 */
typedef struct codegen_x86_64_local_list
{
    codegen_x86_64_local *values;
    unsigned long count;

} codegen_x86_64_local_list;


/*
 * x86-64 function code generation state.
 *
 * Generic codegen owns the actual assembly buffer.
 * The x86-64 backend only receives a pointer to it and
 * appends target-specific assembly.
 */
typedef struct codegen_x86_64_context
{
    /*
     * Assembly output owned by generic codegen.
     */
    codegen_output *output;


    /*
     * Function locals and parameters.
     */
    codegen_x86_64_local_list locals;
    ast_list *structs;
    ast_list *globals;
    ast_list *functions;
    ast *current_function;
    const ir_function *ir_function;


    /*
     * Total stack space required by this function.
     */
    unsigned long stack_size;

    long return_buffer_offset;
    int has_return_buffer;


    /*
     * Temporary stack depth used while evaluating
     * expressions.
     */
    unsigned long stack_depth;


    /*
     * Temporary labels used by this backend.
     */
    unsigned long label_count;
    unsigned long epilogue_label;
    int has_epilogue_label;

    int optimize;
    int debug;
    unsigned long debug_file_id;

} codegen_x86_64_context;


/*
 * Initialize an x86-64 code generation context.
 *
 * The assembly buffer is owned by generic codegen.
 */
void
codegen_x86_64_context_init(
    codegen_x86_64_context *context,
    codegen_output *output
);


/*
 * Free all function-local state.
 */
void
codegen_x86_64_context_free(
    codegen_x86_64_context *context
);

void
codegen_x86_64_emit_location(
    codegen_x86_64_context *context,
    const ast *node
);


/*
 * Add a local variable to the current function.
 *
 * The source-level type information is stored with the local
 * so expression generation can emit type-correct loads and
 * stores.
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
);

/*
 * Find a local variable by name.
 *
 * Searches backwards so that the most recently declared
 * variable wins when names are shadowed.
 */
codegen_x86_64_local *
codegen_x86_64_find_local(
    codegen_x86_64_context *context,
    const char *name
);

unsigned long
codegen_x86_64_context_type_size(
    codegen_x86_64_context *context,
    const char *type,
    unsigned long pointer_depth
);

unsigned long
codegen_x86_64_context_type_alignment(
    codegen_x86_64_context *context,
    const char *type,
    unsigned long pointer_depth
);

ast *
codegen_x86_64_find_struct(
    codegen_x86_64_context *context,
    const char *name
);

ast *
codegen_x86_64_find_global(
    codegen_x86_64_context *context,
    const char *name
);

ast *
codegen_x86_64_find_struct_field(
    ast *structure,
    const char *name
);

int
codegen_x86_64_struct_field_offset(
    codegen_x86_64_context *context,
    const char *struct_name,
    const char *field_name,
    unsigned long *offset
);

#endif
