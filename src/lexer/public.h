#ifndef LOWC_LEXER_PUBLIC_H
#define LOWC_LEXER_PUBLIC_H

#include "../files_handler/public.h"

typedef enum token_type
{
    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_STRING,

    TOKEN_TYPE,
    TOKEN_VOID,

    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_RETURN,
    TOKEN_STRUCT,

    TOKEN_INCLUDE,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_ADDRESS,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    TOKEN_NOT,
    TOKEN_AND,
    TOKEN_OR,

    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    TOKEN_ASSIGN,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_DOT,
    TOKEN_ARROW
} token_type;

typedef struct token
{
    token_type type;

    char *value;

    char *file;

    unsigned long line;
    unsigned long column;
} token;

typedef struct token_stream
{
    token *values;
    unsigned long count;
} token_stream;

typedef struct lexer_result
{
    int error;

    char *error_text;

    char *file;

    unsigned long line;
    unsigned long column;

    char *source_line;
    char *correct;

    token_stream *streams;
    unsigned long count;
} lexer_result;

lexer_result
lexer(
    files_handler_result files
);

void
lexer_handle_error(
    lexer_result result
);

void
lexer_print_tokens(
    lexer_result result
);

#endif
