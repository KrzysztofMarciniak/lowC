#ifndef LOWC_IR_PUBLIC_H
#define LOWC_IR_PUBLIC_H

#include "../parser/public.h"

#include <stdint.h>

typedef struct ir_constant
{
    const ast *expression;
    uint64_t value;
} ir_constant;

typedef struct ir_type
{
    const char *name;
    unsigned long pointer_depth;
    int is_array;
    const ast *array_length;
} ir_type;

typedef enum ir_value_opcode
{
    IR_VALUE_INTEGER,
    IR_VALUE_STRING,
    IR_VALUE_LOAD,
    IR_VALUE_UNARY,
    IR_VALUE_BINARY,
    IR_VALUE_CALL,
    IR_VALUE_INDEX,
    IR_VALUE_MEMBER,
    IR_VALUE_POINTER_MEMBER
} ir_value_opcode;

typedef struct ir_value
{
    unsigned long id;
    ir_value_opcode opcode;
    ir_type type;
    const ast *source;
    const char *name;
    const char *literal;
    ast_unary_operator unary_operator;
    ast_binary_operator binary_operator;
    unsigned long left;
    unsigned long right;
    unsigned long *arguments;
    unsigned long argument_count;
    int is_lvalue;
    int is_tuple;
    int is_constant;
    uint64_t constant_value;
} ir_value;

typedef enum ir_instruction_opcode
{
    IR_INSTRUCTION_DECLARATION,
    IR_INSTRUCTION_EXPRESSION,
    IR_INSTRUCTION_ASSIGNMENT,
    IR_INSTRUCTION_MULTI_ASSIGNMENT,
    IR_INSTRUCTION_RETURN,
    IR_INSTRUCTION_ASSEMBLY
} ir_instruction_opcode;

typedef struct ir_instruction
{
    ir_instruction_opcode opcode;
    const ast *source;
    const char *file;
    unsigned long line;
    unsigned long column;
    const char *symbol_name;
    const char *assembly_template;
    const char **output_constraints;
    unsigned long output_count;
    const char **input_constraints;
    unsigned long input_count;
    const char **clobbers;
    unsigned long clobber_count;
    ir_type type;
    unsigned long result_value;
    unsigned long left_value;
    unsigned long right_value;
    unsigned long *operand_values;
    unsigned long operand_count;
    uint64_t *constant_values;
    unsigned long constant_value_count;
} ir_instruction;

typedef enum ir_terminator_kind
{
    IR_TERMINATOR_NONE,
    IR_TERMINATOR_JUMP,
    IR_TERMINATOR_BRANCH
} ir_terminator_kind;

typedef struct ir_terminator
{
    ir_terminator_kind kind;
    unsigned long condition_value;
    unsigned long true_block;
    unsigned long false_block;
} ir_terminator;

typedef struct ir_basic_block
{
    ir_instruction *instructions;
    unsigned long instruction_count;
    ir_terminator terminator;
} ir_basic_block;

typedef struct ir_function
{
    ast *declaration;
    ir_value *values;
    unsigned long value_count;
    ir_basic_block *blocks;
    unsigned long block_count;
} ir_function;

typedef struct ir_module
{
    ast *source_program;
    ast_list structs;
    ast_list globals;
    ast_list functions;
    ir_function *lowered_functions;
    unsigned long lowered_function_count;
    ir_constant *constants;
    unsigned long constant_count;
} ir_module;

typedef struct ir_result
{
    int error;
    char *error_text;
    ir_module *module;
} ir_result;

ir_result ir_lower(ast *program, int optimize);

void ir_module_free(ir_module *module);

void ir_result_free(ir_result *result);

#endif
