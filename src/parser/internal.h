#ifndef LOWC_PARSER_INTERNAL_H
#define LOWC_PARSER_INTERNAL_H

#include "public.h"

typedef struct parser_state
{
    token_stream *stream;

    unsigned long position;
    unsigned long count;

    /*
     * First syntax error encountered by the parser.
     */
    char *error_text;
    token *error_token;

} parser_state;

token *
parser_current(
    parser_state *state
);

token *
parser_peek(
    parser_state *state,
    unsigned long offset
);

token *
parser_advance(
    parser_state *state
);

int
parser_match(
    parser_state *state,
    token_type type
);

int
parser_expect(
    parser_state *state,
    token_type type
);

void
parser_set_error(
    parser_state *state,
    const char *message
);

const char *
parser_token_name(
    token_type type
);

ast *
parser_parse_program(
    parser_state *state
);

ast *
parser_parse_top_level(
    parser_state *state
);

ast *
parser_parse_import(
    parser_state *state
);

ast *
parser_parse_struct(
    parser_state *state
);

ast *
parser_parse_function_or_global(
    parser_state *state
);

ast *
parser_parse_block(
    parser_state *state
);

ast *
parser_parse_statement(
    parser_state *state
);

ast *
parser_parse_variable_declaration(
    parser_state *state
);

ast *
parser_parse_if(
    parser_state *state
);

ast *
parser_parse_for(
    parser_state *state
);

ast *
parser_parse_return(
    parser_state *state
);

ast *
parser_parse_expression_statement(
    parser_state *state
);

ast *
parser_parse_expression(
    parser_state *state
);

ast *
parser_parse_assignment(
    parser_state *state
);

ast *
parser_parse_logical_or(
    parser_state *state
);

ast *
parser_parse_logical_and(
    parser_state *state
);

ast *
parser_parse_equality(
    parser_state *state
);

ast *
parser_parse_comparison(
    parser_state *state
);

ast *
parser_parse_term(
    parser_state *state
);

ast *
parser_parse_factor(
    parser_state *state
);

ast *
parser_parse_unary(
    parser_state *state
);

ast *
parser_parse_postfix(
    parser_state *state
);

ast *
parser_parse_primary(
    parser_state *state
);

ast *
parser_make_node(
    ast_type type,
    token *token
);

char *
parser_copy_string(
    const char *value
);

void
parser_free_list(
    ast_list *list
);

#endif
