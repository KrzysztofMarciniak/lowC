#ifndef LOWC_PARSER_PUBLIC_H
#define LOWC_PARSER_PUBLIC_H

#include "../lexer/public.h"

typedef enum ast_type
{
    AST_PROGRAM,

    AST_IMPORT,

    AST_STRUCT,
    AST_STRUCT_FIELD,

    AST_GLOBAL,
    AST_FUNCTION,
    AST_PARAMETER,

    AST_BLOCK,

    AST_VARIABLE_DECLARATION,
    AST_EXPRESSION_STATEMENT,

    AST_IF,
    AST_FOR,
    AST_RETURN,

    AST_ASSIGNMENT,

    AST_INTEGER,
    AST_STRING,
    AST_IDENTIFIER,

    AST_UNARY,
    AST_BINARY,

    AST_CALL,
    AST_INDEX,
    AST_MEMBER,
    AST_POINTER_MEMBER
} ast_type;

typedef enum ast_unary_operator
{
    AST_UNARY_PLUS,
    AST_UNARY_MINUS,
    AST_UNARY_NOT,
    AST_UNARY_ADDRESS,
    AST_UNARY_DEREFERENCE
} ast_unary_operator;

typedef enum ast_binary_operator
{
    AST_BINARY_ADD,
    AST_BINARY_SUBTRACT,
    AST_BINARY_MULTIPLY,
    AST_BINARY_DIVIDE,
    AST_BINARY_MODULO,

    AST_BINARY_EQUAL,
    AST_BINARY_NOT_EQUAL,

    AST_BINARY_LESS,
    AST_BINARY_LESS_EQUAL,
    AST_BINARY_GREATER,
    AST_BINARY_GREATER_EQUAL,

    AST_BINARY_AND,
    AST_BINARY_OR
} ast_binary_operator;

typedef struct ast ast;

typedef struct ast_list
{
    ast **values;
    unsigned long count;
} ast_list;

struct ast
{
    ast_type type;

    /*
     * Location of the AST node in the source file.
     */
    char *file;
    unsigned long line;
    unsigned long column;

    union
    {
        /* AST_PROGRAM */
        struct
        {
            ast_list imports;
            ast_list structs;
            ast_list globals;
            ast_list functions;
        } program;

        /* AST_IMPORT */
        struct
        {
            char *path;
            ast_list names;
        } import;

        /* AST_STRUCT */
        struct
        {
            char *name;
            ast_list fields;
        } structure;

        /* AST_STRUCT_FIELD */
        struct
        {
            char *type;
            char *name;
            unsigned long pointer_depth;
	    ast *array_length;   /* NULL when not an array */
        } struct_field;

        /* AST_GLOBAL / AST_VARIABLE_DECLARATION */
        struct
        {
            char *type;
            char *name;
            unsigned long pointer_depth;
            ast *value;
	    ast *array_length;   /* NULL when not an array */
        } variable;

        /* AST_EXPRESSION_STATEMENT */
        struct
        {
            ast *expression;
        } expression_statement;

        /* AST_FUNCTION */
        struct
        {
            char *return_type;
            unsigned long return_pointer_depth;
            char *name;
            ast_list parameters;
            ast *body;
        } function;

        /* AST_PARAMETER */
        struct
        {
            char *type;
            char *name;
            unsigned long pointer_depth;
        } parameter;

        /* AST_BLOCK */
        struct
        {
            ast_list statements;
        } block;

        /* AST_IF */
        struct
        {
            ast *condition;
            ast *then_body;
            ast *else_body;
        } if_statement;

        /* AST_FOR */
        struct
        {
            ast *init;
            ast *condition;
            ast *increment;
            ast *body;
        } for_statement;

        /* AST_RETURN */
        struct
        {
            ast *value;
        } return_statement;

        /* AST_ASSIGNMENT */
        struct
        {
            ast *left;
            ast *right;
        } assignment;

        /* AST_INTEGER */
        struct
        {
            char *value;
        } integer;

        /* AST_STRING */
        struct
        {
            char *value;
        } string;

        /* AST_IDENTIFIER */
        struct
        {
            char *name;
        } identifier;

        /* AST_UNARY */
        struct
        {
            ast_unary_operator operator;
            ast *operand;
        } unary;

        /* AST_BINARY */
        struct
        {
            ast_binary_operator operator;
            ast *left;
            ast *right;
        } binary;

        /* AST_CALL */
        struct
        {
            ast *function;
            ast_list arguments;
        } call;

        /* AST_INDEX */
        struct
        {
            ast *object;
            ast *index;
        } index;

        /* AST_MEMBER */
        struct
        {
            ast *object;
            char *name;
        } member;

        /* AST_POINTER_MEMBER */
        struct
        {
            ast *object;
            char *name;
        } pointer_member;
    };
};

typedef struct parser_result
{
    int error;
    char *error_text;

    /*
     * Location of the syntax error.
     */
    char *file;
    unsigned long line;
    unsigned long column;

    ast *tree;
} parser_result;

parser_result
parser(
    lexer_result lexer
);

void
parser_handle_error(
    parser_result result
);

void
parser_free(
    ast *tree
);

void
parser_print_ast(
    ast *tree
);

#endif
