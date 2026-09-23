#ifndef LOWC_LEXER_INTERNAL_H
#define LOWC_LEXER_INTERNAL_H

#include "public.h"

typedef struct lexer_state
{
    source_file *file;

    unsigned long position;
    unsigned long line;
    unsigned long column;
} lexer_state;

token
lexer_next_token(
    lexer_state *state
);

void
lexer_free_token(
    token *token
);

void
lexer_free_streams(
    token_stream *streams,
    unsigned long count
);

lexer_result
lexer_make_error(
    lexer_state *state,
    const char *error_text,
    const char *correct
);

#endif
