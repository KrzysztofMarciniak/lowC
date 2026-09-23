#ifndef LOWC_CODEGEN_TARGET_H
#define LOWC_CODEGEN_TARGET_H

#include "output.h"

typedef struct codegen_context codegen_context;

typedef enum codegen_binary_operator
{
    CODEGEN_BINARY_ADD,
    CODEGEN_BINARY_SUBTRACT,
    CODEGEN_BINARY_MULTIPLY,
    CODEGEN_BINARY_DIVIDE,
    CODEGEN_BINARY_MODULO,

    CODEGEN_BINARY_EQUAL,
    CODEGEN_BINARY_NOT_EQUAL,

    CODEGEN_BINARY_LESS,
    CODEGEN_BINARY_LESS_EQUAL,
    CODEGEN_BINARY_GREATER,
    CODEGEN_BINARY_GREATER_EQUAL,

    CODEGEN_BINARY_AND,
    CODEGEN_BINARY_OR

} codegen_binary_operator;

typedef enum codegen_unary_operator
{
    CODEGEN_UNARY_PLUS,
    CODEGEN_UNARY_MINUS,
    CODEGEN_UNARY_NOT

} codegen_unary_operator;

typedef struct codegen_target
{
    const char *name;

    int (*begin)(
        codegen_context *context
    );

    int (*end)(
        codegen_context *context
    );

    /*
     * Functions
     */

    int (*function_begin)(
        codegen_context *context,
        const char *name
    );

    int (*function_end)(
        codegen_context *context
    );

    /*
     * Control flow
     */

    int (*label)(
        codegen_context *context,
        const char *name
    );

    int (*jump)(
        codegen_context *context,
        const char *name
    );

    int (*jump_zero)(
        codegen_context *context,
        const char *name
    );

    /*
     * Values
     */

    int (*integer)(
        codegen_context *context,
        const char *value
    );

    int (*load)(
        codegen_context *context,
        const char *name
    );

    int (*store)(
        codegen_context *context,
        const char *name
    );

    /*
     * Operators
     */

    int (*unary)(
        codegen_context *context,
        codegen_unary_operator operator
    );

    int (*binary)(
        codegen_context *context,
        codegen_binary_operator operator
    );

    /*
     * Memory
     */

    int (*address)(
        codegen_context *context,
        const char *name
    );

    int (*load_pointer)(
        codegen_context *context
    );

    int (*store_pointer)(
        codegen_context *context
    );

    /*
     * Calls
     */

    int (*call)(
        codegen_context *context,
        const char *name,
        unsigned long arguments
    );

    /*
     * Return
     */

    int (*return_value)(
        codegen_context *context
    );

    int (*return_void)(
        codegen_context *context
    );

} codegen_target;
codegen_output *codegen_target_output(
    codegen_context *context
);

#endif
