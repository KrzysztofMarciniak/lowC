#include "public.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

static lexer_result
lexer_error(
    lexer_state *state,
    const char *error_text,
    const char *correct,
    token_stream *streams,
    unsigned long count
)
{
    lexer_result result =
        lexer_make_error(
            state,
            error_text,
            correct
        );

    result.streams = streams;
    result.count = count;

    return result;
}

lexer_result
lexer(
    files_handler_result files
)
{
    token_stream *streams =
        calloc(
            files.count,
            sizeof(token_stream)
        );

    if (files.count > 0 && !streams)
    {
        return (lexer_result) {
            .error = 1,
            .error_text =
                "failed to allocate token streams."
        };
    }

    for (
        unsigned long file_index = 0;
        file_index < (unsigned long)files.count;
        file_index++
    )
    {
        lexer_state state = {
            .file =
                &files.values[file_index],

            .position = 0,
            .line = 1,
            .column = 1
        };

        unsigned long capacity = 16;

        streams[file_index].values =
            calloc(
                capacity,
                sizeof(token)
            );

        if (!streams[file_index].values)
        {
            lexer_free_streams(
                streams,
                file_index
            );

            return (lexer_result) {
                .error = 1,
                .error_text =
                    "failed to allocate tokens."
            };
        }

        while (1)
        {
            if (
                streams[file_index].count >=
                capacity
            )
            {
                capacity *= 2;

                token *new_values =
                    realloc(
                        streams[file_index].values,
                        capacity * sizeof(token)
                    );

                if (!new_values)
                {
                    lexer_free_streams(
                        streams,
                        file_index + 1
                    );

                    return (lexer_result) {
                        .error = 1,
                        .error_text =
                            "failed to allocate tokens."
                    };
                }

                streams[file_index].values =
                    new_values;
            }

            token next =
                lexer_next_token(&state);

            if (next.type == TOKEN_ERROR)
            {
                lexer_free_token(&next);

                lexer_free_streams(
                    streams,
                    file_index + 1
                );

                return lexer_error(
                    &state,
                    "invalid character or token.",
                    "remove or replace the invalid character.",
                    0,
                    0
                );
            }

            streams[file_index].values[
                streams[file_index].count
            ] = next;

            streams[file_index].count++;

            if (next.type == TOKEN_EOF)
                break;
        }
    }

    return (lexer_result) {
        .error = 0,
        .error_text = 0,
        .file = 0,
        .line = 0,
        .column = 0,
        .source_line = 0,
        .correct = 0,
        .streams = streams,
        .count = files.count
    };
}

void
lexer_handle_error(
    lexer_result result
)
{
    fprintf(
        stderr,
        "error: %s\n",
        result.error_text
            ? result.error_text
            : "unknown lexer error."
    );

    if (result.file)
    {
        fprintf(
            stderr,
            "file: %s\n",
            result.file
        );
    }

    if (result.line)
    {
        fprintf(
            stderr,
            "line: %lu\n",
            result.line
        );
    }

    if (result.source_line)
    {
        fprintf(
            stderr,
            "\n"
            "incorrectly done:\n"
            "    %s\n"
            "    %*s^\n",
            result.source_line,
            (int)(result.column - 1),
            ""
        );
    }

    if (result.correct)
    {
        fprintf(
            stderr,
            "\n"
            "correct:\n"
            "    %s\n",
            result.correct
        );
    }

    fprintf(
        stderr,
        "\n"
        "for further support visit README.md "
        "or --help.\n"
    );
}

void
lexer_print_tokens(
    lexer_result result
)
{
    for (
        unsigned long stream_index = 0;
        stream_index < result.count;
        stream_index++
    )
    {
        token_stream *stream =
            &result.streams[stream_index];

        printf(
            "=== token stream %lu ===\n",
            stream_index
        );

        for (
            unsigned long token_index = 0;
            token_index < stream->count;
            token_index++
        )
        {
            token *current =
                &stream->values[token_index];

            printf(
                "[%lu:%lu] ",
                current->line,
                current->column
            );

            switch (current->type)
            {
                case TOKEN_EOF:
                    printf("EOF");
                    break;

                case TOKEN_ERROR:
                    printf("ERROR");
                    break;

                case TOKEN_IDENTIFIER:
                    printf("IDENTIFIER");
                    break;

                case TOKEN_INTEGER:
                    printf("INTEGER");
                    break;

                case TOKEN_STRING:
                    printf("STRING");
                    break;

                case TOKEN_TYPE:
                    printf("TYPE");
                    break;

                case TOKEN_VOID:
                    printf("VOID");
                    break;

                case TOKEN_IF:
                    printf("IF");
                    break;

                case TOKEN_ELSE:
                    printf("ELSE");
                    break;

                case TOKEN_FOR:
                    printf("FOR");
                    break;

                case TOKEN_RETURN:
                    printf("RETURN");
                    break;

                case TOKEN_STRUCT:
                    printf("STRUCT");
                    break;

                case TOKEN_INCLUDE:
                    printf("INCLUDE");
                    break;

                case TOKEN_PLUS:
                    printf("PLUS");
                    break;

                case TOKEN_MINUS:
                    printf("MINUS");
                    break;

                case TOKEN_STAR:
                    printf("STAR");
                    break;

                case TOKEN_ADDRESS:
                    printf("ADDRESS");
                    break;

                case TOKEN_SLASH:
                    printf("SLASH");
                    break;

                case TOKEN_PERCENT:
                    printf("PERCENT");
                    break;

                case TOKEN_NOT:
                    printf("NOT");
                    break;

                case TOKEN_AND:
                    printf("AND");
                    break;

                case TOKEN_OR:
                    printf("OR");
                    break;

                case TOKEN_EQUAL:
                    printf("EQUAL");
                    break;

                case TOKEN_NOT_EQUAL:
                    printf("NOT_EQUAL");
                    break;

                case TOKEN_LESS:
                    printf("LESS");
                    break;

                case TOKEN_LESS_EQUAL:
                    printf("LESS_EQUAL");
                    break;

                case TOKEN_GREATER:
                    printf("GREATER");
                    break;

                case TOKEN_GREATER_EQUAL:
                    printf("GREATER_EQUAL");
                    break;

                case TOKEN_ASSIGN:
                    printf("ASSIGN");
                    break;

                case TOKEN_LPAREN:
                    printf("LPAREN");
                    break;

                case TOKEN_RPAREN:
                    printf("RPAREN");
                    break;

                case TOKEN_LBRACE:
                    printf("LBRACE");
                    break;

                case TOKEN_RBRACE:
                    printf("RBRACE");
                    break;

                case TOKEN_LBRACKET:
                    printf("LBRACKET");
                    break;

                case TOKEN_RBRACKET:
                    printf("RBRACKET");
                    break;

                case TOKEN_SEMICOLON:
                    printf("SEMICOLON");
                    break;

                case TOKEN_COMMA:
                    printf("COMMA");
                    break;

                case TOKEN_COLON:
                    printf("COLON");
                    break;

                case TOKEN_DOT:
                    printf("DOT");
                    break;

                case TOKEN_ARROW:
                    printf("ARROW");
                    break;

                default:
                    printf("UNKNOWN");
                    break;
            }

            if (current->value)
            {
                printf(
                    " \"%s\"",
                    current->value
                );
            }

            if (current->file)
            {
                printf(
                    " (%s)",
                    current->file
                );
            }

            printf("\n");
        }

        printf("\n");
    }
}
