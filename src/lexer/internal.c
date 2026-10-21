#include "internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
lexer_strdup(
    const char *value
)
{
    char *copy;

    if (value == NULL)
        return NULL;

    copy = malloc(strlen(value) + 1);

    if (copy == NULL)
        return NULL;

    strcpy(copy, value);

    return copy;
}


static char
lexer_current(
    lexer_state *state
)
{
    if (state->position >= state->file->size)
        return '\0';

    return state->file->content[state->position];
}

static char
lexer_peek(
    lexer_state *state
)
{
    if (state->position + 1 >= state->file->size)
        return '\0';

    return state->file->content[state->position + 1];
}

static void
lexer_advance(
    lexer_state *state
)
{
    if (lexer_current(state) == '\0')
        return;

    if (lexer_current(state) == '\n')
    {
        state->line++;
        state->column = 1;
    }
    else
    {
        state->column++;
    }

    state->position++;
}

static char *
lexer_copy(
    lexer_state *state,
    unsigned long start,
    unsigned long end
)
{
    unsigned long size = end - start;

    char *value = malloc(size + 1);

    if (!value)
        return 0;

    memcpy(
        value,
        &state->file->content[start],
        size
    );

    value[size] = '\0';

    return value;
}

static char *
lexer_get_source_line(
    lexer_state *state
)
{
    unsigned long start = state->position;

    while (
        start > 0 &&
        state->file->content[start - 1] != '\n'
    )
    {
        start--;
    }

    unsigned long end = state->position;

    while (
        end < state->file->size &&
        state->file->content[end] != '\n'
    )
    {
        end++;
    }

    return lexer_copy(
        state,
        start,
        end
    );
}

lexer_result
lexer_make_error(
    lexer_state *state,
    const char *error_text,
    const char *correct
)
{
    char *source_line =
        lexer_get_source_line(state);

    return (lexer_result) {
        .error = 1,

        .error_text =
            (char *)error_text,

        .file =
            state->file->path,

        .line =
            state->line,

        .column =
            state->column,

        .source_line =
            source_line,

        .correct =
            (char *)correct,

        .streams = 0,
        .count = 0
    };
}

static token
lexer_make_token(
    lexer_state *state,
    token_type type,
    unsigned long start,
    unsigned long line,
    unsigned long column
)
{
    return (token) {
        .type = type,

        .value =
            lexer_copy(
                state,
                start,
                state->position
            ),

        .file =
            state->file->path,

        .line =
            line,

        .column =
            column
    };
}

static void
lexer_skip_whitespace(
    lexer_state *state
)
{
    while (
        isspace(
            (unsigned char)lexer_current(state)
        )
    )
    {
        lexer_advance(state);
    }
}

static void
lexer_skip_line_comment(
    lexer_state *state
)
{
    while (
        lexer_current(state) != '\0' &&
        lexer_current(state) != '\n'
    )
    {
        lexer_advance(state);
    }
}

static int
lexer_skip_block_comment(
    lexer_state *state
)
{
    lexer_advance(state);
    lexer_advance(state);

    while (lexer_current(state) != '\0')
    {
        if (
            lexer_current(state) == '*' &&
            lexer_peek(state) == '/'
        )
        {
            lexer_advance(state);
            lexer_advance(state);

            return 0;
        }

        lexer_advance(state);
    }

    return 1;
}

static token_type
lexer_keyword(
    const char *value
)
{
    if (
        strcmp(value, "u1") == 0 ||
        strcmp(value, "u2") == 0 ||
        strcmp(value, "u4") == 0 ||
        strcmp(value, "u8") == 0 ||
        strcmp(value, "s1") == 0 ||
        strcmp(value, "s2") == 0 ||
        strcmp(value, "s4") == 0 ||
        strcmp(value, "s8") == 0
    )
    {
        return TOKEN_TYPE;
    }

    if (strcmp(value, "void") == 0)
        return TOKEN_VOID;

    if (strcmp(value, "if") == 0)
        return TOKEN_IF;

    if (strcmp(value, "else") == 0)
        return TOKEN_ELSE;

    if (strcmp(value, "for") == 0)
        return TOKEN_FOR;

    if (strcmp(value, "return") == 0)
        return TOKEN_RETURN;

    if (strcmp(value, "struct") == 0)
        return TOKEN_STRUCT;

    if (strcmp(value, "asm") == 0)
        return TOKEN_ASM;

    return TOKEN_IDENTIFIER;
}

static token
lexer_identifier(
    lexer_state *state,
    unsigned long line,
    unsigned long column
)
{
    unsigned long start =
        state->position;

    while (
        isalnum(
            (unsigned char)lexer_current(state)
        ) ||
        lexer_current(state) == '_'
    )
    {
        lexer_advance(state);
    }

    char *value =
        lexer_copy(
            state,
            start,
            state->position
        );

    if (!value)
    {
        return (token) {
            .type = TOKEN_ERROR,
            .file = state->file->path,
            .line = line,
            .column = column
        };
    }

    token_type type =
        lexer_keyword(value);

    return (token) {
        .type = type,
        .value = value,
        .file = state->file->path,
        .line = line,
        .column = column
    };
}

static token
lexer_integer(
    lexer_state *state,
    unsigned long line,
    unsigned long column
)
{
    unsigned long start =
        state->position;

    while (
        isdigit(
            (unsigned char)lexer_current(state)
        )
    )
    {
        lexer_advance(state);
    }

    return lexer_make_token(
        state,
        TOKEN_INTEGER,
        start,
        line,
        column
    );
}

static token
lexer_string(
    lexer_state *state,
    unsigned long line,
    unsigned long column
)
{
    lexer_advance(state);

    unsigned long start =
        state->position;

    while (
        lexer_current(state) != '\0' &&
        lexer_current(state) != '"'
    )
    {
        if (lexer_current(state) == '\\' && lexer_peek(state) != '\0')
        {
            lexer_advance(state);
            lexer_advance(state);
            continue;
        }

        if (lexer_current(state) == '\n')
        {
            return (token) {
                .type = TOKEN_ERROR,
                .file = state->file->path,
                .line = line,
                .column = column
            };
        }

        lexer_advance(state);
    }

    if (lexer_current(state) != '"')
    {
        return (token) {
            .type = TOKEN_ERROR,
            .file = state->file->path,
            .line = line,
            .column = column
        };
    }

    token result =
        lexer_make_token(
            state,
            TOKEN_STRING,
            start,
            line,
            column
        );

    lexer_advance(state);

    return result;
}

token
lexer_next_token(
    lexer_state *state
)
{
    while (1)
    {
        lexer_skip_whitespace(state);

        if (
            lexer_current(state) == '/' &&
            lexer_peek(state) == '/'
        )
        {
            lexer_skip_line_comment(state);
            continue;
        }

        if (
            lexer_current(state) == '/' &&
            lexer_peek(state) == '*'
        )
        {
            if (lexer_skip_block_comment(state))
            {
                return (token) {
                    .type = TOKEN_ERROR,
                    .file = state->file->path,
                    .line = state->line,
                    .column = state->column
                };
            }

            continue;
        }

        break;
    }

    unsigned long line =
        state->line;

    unsigned long column =
        state->column;

    unsigned long start =
        state->position;

    char current =
        lexer_current(state);

    char next =
        lexer_peek(state);

    if (current == '\0')
    {
        return (token) {
            .type = TOKEN_EOF,
            .file = state->file->path,
            .line = line,
            .column = column
        };
    }

    if (
        isalpha((unsigned char)current) ||
        current == '_'
    )
    {
        return lexer_identifier(
            state,
            line,
            column
        );
    }

    if (isdigit((unsigned char)current))
    {
        return lexer_integer(
            state,
            line,
            column
        );
    }

    if (current == '"')
    {
        return lexer_string(
            state,
            line,
            column
        );
    }

    if (current == '#')
    {
        if (
            strncmp(
                &state->file->content[state->position],
                "#include",
                8
            ) == 0
        )
        {
            for (int index = 0; index < 8; index++)
                lexer_advance(state);

            return (token) {
                .type = TOKEN_INCLUDE,
                .value = lexer_strdup("#include"),
                .file = state->file->path,
                .line = line,
                .column = column
            };
        }

        lexer_advance(state);

        return (token) {
            .type = TOKEN_ERROR,
            .file = state->file->path,
            .line = line,
            .column = column
        };
    }

    if (current == '=' && next == '=')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_EQUAL,
            start,
            line,
            column
        );
    }

    if (current == '!' && next == '=')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_NOT_EQUAL,
            start,
            line,
            column
        );
    }

    if (current == '<' && next == '=')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_LESS_EQUAL,
            start,
            line,
            column
        );
    }

    if (current == '>' && next == '=')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_GREATER_EQUAL,
            start,
            line,
            column
        );
    }

    if (current == '-' && next == '>')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_ARROW,
            start,
            line,
            column
        );
    }

    if (current == '&' && next == '&')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_AND,
            start,
            line,
            column
        );
    }

    if (current == '|' && next == '|')
    {
        lexer_advance(state);
        lexer_advance(state);

        return lexer_make_token(
            state,
            TOKEN_OR,
            start,
            line,
            column
        );
    }

    lexer_advance(state);

    switch (current)
    {
        case '+':
            return lexer_make_token(
                state,
                TOKEN_PLUS,
                start,
                line,
                column
            );

        case '-':
            return lexer_make_token(
                state,
                TOKEN_MINUS,
                start,
                line,
                column
            );

	case '&':
	    return lexer_make_token(
		state,
		TOKEN_ADDRESS,
		start,
		line,
		column
	    );

        case '*':
            return lexer_make_token(
                state,
                TOKEN_STAR,
                start,
                line,
                column
            );

        case '/':
            return lexer_make_token(
                state,
                TOKEN_SLASH,
                start,
                line,
                column
            );

        case '%':
            return lexer_make_token(
                state,
                TOKEN_PERCENT,
                start,
                line,
                column
            );

        case '!':
            return lexer_make_token(
                state,
                TOKEN_NOT,
                start,
                line,
                column
            );

        case '<':
            return lexer_make_token(
                state,
                TOKEN_LESS,
                start,
                line,
                column
            );

        case '>':
            return lexer_make_token(
                state,
                TOKEN_GREATER,
                start,
                line,
                column
            );

        case '=':
            return lexer_make_token(
                state,
                TOKEN_ASSIGN,
                start,
                line,
                column
            );

        case '(':
            return lexer_make_token(
                state,
                TOKEN_LPAREN,
                start,
                line,
                column
            );

        case ')':
            return lexer_make_token(
                state,
                TOKEN_RPAREN,
                start,
                line,
                column
            );

        case '{':
            return lexer_make_token(
                state,
                TOKEN_LBRACE,
                start,
                line,
                column
            );

        case '}':
            return lexer_make_token(
                state,
                TOKEN_RBRACE,
                start,
                line,
                column
            );

        case '[':
            return lexer_make_token(
                state,
                TOKEN_LBRACKET,
                start,
                line,
                column
            );

        case ']':
            return lexer_make_token(
                state,
                TOKEN_RBRACKET,
                start,
                line,
                column
            );

        case ';':
            return lexer_make_token(
                state,
                TOKEN_SEMICOLON,
                start,
                line,
                column
            );

        case ',':
            return lexer_make_token(
                state,
                TOKEN_COMMA,
                start,
                line,
                column
            );

        case ':':
            return lexer_make_token(
                state,
                TOKEN_COLON,
                start,
                line,
                column
            );

        case '.':
            return lexer_make_token(
                state,
                TOKEN_DOT,
                start,
                line,
                column
            );

        default:
            return (token) {
                .type = TOKEN_ERROR,
                .file = state->file->path,
                .line = line,
                .column = column
            };
    }
}

void
lexer_free_token(
    token *token
)
{
    free(token->value);
}

void
lexer_free_streams(
    token_stream *streams,
    unsigned long count
)
{
    if (!streams)
        return;

    for (unsigned long i = 0; i < count; i++)
    {
        for (
            unsigned long j = 0;
            j < streams[i].count;
            j++
        )
        {
            lexer_free_token(
                &streams[i].values[j]
            );
        }

        free(streams[i].values);
    }

    free(streams);
}
