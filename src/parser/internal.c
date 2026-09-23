#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static ast *
parser_new_node(
    ast_type type,
    token *token
)
{
    ast *node;

    node = calloc(1, sizeof(ast));

    if (node == NULL)
        return NULL;

    node->type = type;

    if (token != NULL)
    {
        node->file =
            parser_copy_string(token->file);

        node->line = token->line;
        node->column = token->column;

        if (token->file != NULL &&
            node->file == NULL)
        {
            free(node);
            return NULL;
        }
    }

    return node;
}

ast *
parser_make_node(
    ast_type type,
    token *token
)
{
    return parser_new_node(type, token);
}

char *
parser_copy_string(
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

token *
parser_current(
    parser_state *state
)
{
    if (state == NULL)
        return NULL;

    if (state->position >= state->count)
        return NULL;

    return &state->stream->values[
        state->position
    ];
}

token *
parser_peek(
    parser_state *state,
    unsigned long offset
)
{
    unsigned long position;

    if (state == NULL)
        return NULL;

    position =
        state->position + offset;

    if (position >= state->count)
        return NULL;

    return &state->stream->values[
        position
    ];
}

token *
parser_advance(
    parser_state *state
)
{
    token *current;

    current =
        parser_current(state);

    if (current != NULL)
        state->position++;

    return current;
}

int
parser_match(
    parser_state *state,
    token_type type
)
{
    token *current;

    current =
        parser_current(state);

    if (current == NULL)
        return 0;

    if (current->type != type)
        return 0;

    state->position++;

    return 1;
}

void
parser_set_error(
    parser_state *state,
    const char *message
)
{
    if (state == NULL)
        return;

    /*
     * Keep the first parser error.
     *
     * Once a syntax error has been found, later
     * parser failures should not overwrite it.
     */
    if (state->error_text != NULL)
        return;

    state->error_text =
        parser_copy_string(message);

    state->error_token =
        parser_current(state);
}

const char *
parser_token_name(
    token_type type
)
{
    switch (type)
    {
        case TOKEN_EOF:
            return "end of file";

        case TOKEN_IDENTIFIER:
            return "identifier";

        case TOKEN_INTEGER:
            return "integer";

        case TOKEN_STRING:
            return "string";

        case TOKEN_TYPE:
            return "type";

        case TOKEN_VOID:
            return "'void'";

        case TOKEN_INCLUDE:
            return "'#include'";

        case TOKEN_STRUCT:
            return "'struct'";

        case TOKEN_IF:
            return "'if'";

        case TOKEN_ELSE:
            return "'else'";

        case TOKEN_FOR:
            return "'for'";

        case TOKEN_RETURN:
            return "'return'";

        case TOKEN_PLUS:
            return "'+'";

        case TOKEN_MINUS:
            return "'-'";

        case TOKEN_STAR:
            return "'*'";

        case TOKEN_SLASH:
            return "'/'";

        case TOKEN_PERCENT:
            return "'%'";

        case TOKEN_NOT:
            return "'!'";

        case TOKEN_ADDRESS:
            return "'&'";

        case TOKEN_EQUAL:
            return "'=='";

        case TOKEN_NOT_EQUAL:
            return "'!='";

        case TOKEN_LESS:
            return "'<'";

        case TOKEN_LESS_EQUAL:
            return "'<='";

        case TOKEN_GREATER:
            return "'>'";

        case TOKEN_GREATER_EQUAL:
            return "'>='";

        case TOKEN_ASSIGN:
            return "'='";

        case TOKEN_AND:
            return "'&&'";

        case TOKEN_OR:
            return "'||'";

        case TOKEN_LPAREN:
            return "'('";

        case TOKEN_RPAREN:
            return "')'";

        case TOKEN_LBRACE:
            return "'{'";

        case TOKEN_RBRACE:
            return "'}'";

        case TOKEN_LBRACKET:
            return "'['";

        case TOKEN_RBRACKET:
            return "']'";

        case TOKEN_SEMICOLON:
            return "';'";

        case TOKEN_COMMA:
            return "','";

        case TOKEN_COLON:
            return "':'";

        case TOKEN_DOT:
            return "'.'";

        case TOKEN_ARROW:
            return "'->'";

        case TOKEN_ERROR:
            return "invalid token";
    }

    return "unknown token";
}

int
parser_expect(
    parser_state *state,
    token_type type
)
{
    token *current;
    char message[256];

    if (parser_match(
            state,
            type
        ))
    {
        return 1;
    }

    current =
        parser_current(state);

    if (current == NULL)
    {
        parser_set_error(
            state,
            "unexpected end of input."
        );

        return 0;
    }

    if (current->value != NULL)
    {
        snprintf(
            message,
            sizeof(message),
            "expected %s, found '%s'.",
            parser_token_name(type),
            current->value
        );
    }
    else
    {
        snprintf(
            message,
            sizeof(message),
            "expected %s, found %s.",
            parser_token_name(type),
            parser_token_name(current->type)
        );
    }

    parser_set_error(
        state,
        message
    );

    return 0;
}

static int
parser_is_type(
    token_type type
)
{
    return type == TOKEN_TYPE ||
           type == TOKEN_VOID ||
           type == TOKEN_STRUCT;
}

static char *
parser_parse_type(
    parser_state *state,
    unsigned long *pointer_depth
)
{
    token *current;
    token *name;
    char *type;

    current =
        parser_current(state);

    if (current == NULL)
    {
        parser_set_error(
            state,
            "expected a type, found end of input."
        );

        return NULL;
    }

    /*
     * Built-in types:
     *
     *     s64
     *     void
     */
    if (
        current->type == TOKEN_TYPE ||
        current->type == TOKEN_VOID
    )
    {
        type =
            parser_copy_string(
                current->value
            );

        if (type == NULL)
            return NULL;

        parser_advance(state);
    }
    /*
     * Named struct type:
     *
     *     struct Point
     *
     * Stored as just the struct's name ("Point"), matching how
     * semantic_struct_create() registers it and how
     * semantic_struct_find() looks it up -- NOT prefixed with
     * "struct ".
     */
    else if (current->type == TOKEN_STRUCT)
    {
        parser_advance(state);

        name =
            parser_current(state);

        if (name == NULL)
        {
            parser_set_error(
                state,
                "expected a struct name, found end of input."
            );

            return NULL;
        }

        if (name->type != TOKEN_IDENTIFIER)
        {
            parser_set_error(
                state,
                "expected a struct name."
            );

            return NULL;
        }

        type =
            parser_copy_string(
                name->value
            );

        if (type == NULL)
            return NULL;

        parser_advance(state);
    }
    else
    {
        parser_set_error(
            state,
            "expected a type."
        );

        return NULL;
    }

    *pointer_depth = 0;

    while (parser_match(
        state,
        TOKEN_STAR
    ))
    {
        (*pointer_depth)++;
    }

    return type;
}

static int
parser_append_ast(
    ast_list *list,
    ast *node
)
{
    ast **values;

    if (list == NULL ||
        node == NULL)
        return 0;

    values =
        realloc(
            list->values,
            sizeof(ast *) *
            (list->count + 1)
        );

    if (values == NULL)
        return 0;

    list->values = values;

    list->values[
        list->count
    ] = node;

    list->count++;

    return 1;
}

ast *
parser_parse_program(
    parser_state *state
)
{
    ast *program;
    ast *node;

    program =
        parser_new_node(
            AST_PROGRAM,
            NULL
        );

    if (program == NULL)
        return NULL;

    while (
        parser_current(state) != NULL &&
        parser_current(state)->type != TOKEN_EOF
    )
    {
        node =
            parser_parse_top_level(state);

        if (node == NULL)
        {
            parser_free(program);
            return NULL;
        }

        if (node->type == AST_IMPORT)
        {
            if (!parser_append_ast(
                    &program->program.imports,
                    node
                ))
            {
                parser_free(node);
                parser_free(program);
                return NULL;
            }
        }
        else if (node->type == AST_STRUCT)
        {
            if (!parser_append_ast(
                    &program->program.structs,
                    node
                ))
            {
                parser_free(node);
                parser_free(program);
                return NULL;
            }
        }
        else if (node->type == AST_FUNCTION)
        {
            if (!parser_append_ast(
                    &program->program.functions,
                    node
                ))
            {
                parser_free(node);
                parser_free(program);
                return NULL;
            }
        }
        else
        {
            if (!parser_append_ast(
                    &program->program.globals,
                    node
                ))
            {
                parser_free(node);
                parser_free(program);
                return NULL;
            }
        }
    }

    return program;
}

ast *
parser_parse_top_level(
    parser_state *state
)
{
    token *current;

    current =
        parser_current(state);

    if (current == NULL)
    {
        parser_set_error(
            state,
            "unexpected end of input."
        );

        return NULL;
    }

    if (current->type == TOKEN_INCLUDE)
        return parser_parse_import(state);

    if (current->type == TOKEN_STRUCT)
        return parser_parse_struct(state);

    return parser_parse_function_or_global(state);
}

ast *
parser_parse_import(
    parser_state *state
)
{
    token *path;
    token *name;
    ast *node;
    ast *name_node;

    parser_advance(state);

    path =
        parser_current(state);

    if (path == NULL)
    {
        parser_set_error(
            state,
            "expected an import path, found end of input."
        );

        return NULL;
    }

    if (path->type != TOKEN_STRING)
    {
        parser_set_error(
            state,
            "expected an import path string."
        );

        return NULL;
    }

    node =
        parser_new_node(
            AST_IMPORT,
            path
        );

    if (node == NULL)
        return NULL;

    node->import.path =
        parser_copy_string(
            path->value
        );

    if (node->import.path == NULL)
    {
        parser_free(node);
        return NULL;
    }

    parser_advance(state);

    if (!parser_expect(
            state,
            TOKEN_COLON
        ))
    {
        parser_free(node);
        return NULL;
    }

    do
    {
        name =
            parser_current(state);

        if (name == NULL)
        {
            parser_set_error(
                state,
                "expected an imported name, found end of input."
            );

            parser_free(node);
            return NULL;
        }

        if (name->type != TOKEN_STRING)
        {
            parser_set_error(
                state,
                "expected an imported name string."
            );

            parser_free(node);
            return NULL;
        }

        name_node =
            parser_new_node(
                AST_IDENTIFIER,
                name
            );

        if (name_node == NULL)
        {
            parser_free(node);
            return NULL;
        }

        name_node->identifier.name =
            parser_copy_string(
                name->value
            );

        if (name_node->identifier.name == NULL)
        {
            parser_free(name_node);
            parser_free(node);
            return NULL;
        }

        if (!parser_append_ast(
                &node->import.names,
                name_node
            ))
        {
            parser_free(name_node);
            parser_free(node);
            return NULL;
        }

        parser_advance(state);

    } while (parser_match(
        state,
        TOKEN_COMMA
    ));

    if (!parser_expect(
            state,
            TOKEN_SEMICOLON
        ))
    {
        parser_free(node);
        return NULL;
    }

    return node;
}

ast *
parser_parse_struct(
    parser_state *state
)
{
    token *name;
    ast *node;

    parser_advance(state);

    name =
        parser_current(state);

    if (name == NULL)
    {
        parser_set_error(
            state,
            "expected a struct name, found end of input."
        );

        return NULL;
    }

    if (name->type != TOKEN_IDENTIFIER)
    {
        parser_set_error(
            state,
            "expected a struct name."
        );

        return NULL;
    }

    node =
        parser_new_node(
            AST_STRUCT,
            name
        );

    if (node == NULL)
        return NULL;

    node->structure.name =
        parser_copy_string(
            name->value
        );

    if (node->structure.name == NULL)
    {
        parser_free(node);
        return NULL;
    }

    parser_advance(state);

    if (!parser_expect(
            state,
            TOKEN_LBRACE
        ))
    {
        parser_free(node);
        return NULL;
    }

    while (1)
    {
        unsigned long pointer_depth;
        char *type;
        token *field_name;
        ast *field;

        if (parser_current(state) == NULL)
        {
            parser_set_error(
                state,
                "unexpected end of input inside struct."
            );

            parser_free(node);
            return NULL;
        }

        if (parser_current(state)->type ==
            TOKEN_RBRACE)
        {
            parser_advance(state);
            break;
        }

        if (parser_current(state)->type ==
            TOKEN_EOF)
        {
            parser_set_error(
                state,
                "unexpected end of input inside struct."
            );

            parser_free(node);
            return NULL;
        }

        type =
            parser_parse_type(
                state,
                &pointer_depth
            );

        if (type == NULL)
        {
            parser_free(node);
            return NULL;
        }

        field_name =
            parser_current(state);

        if (field_name == NULL)
        {
            free(type);

            parser_set_error(
                state,
                "expected a struct field name, found end of input."
            );

            parser_free(node);
            return NULL;
        }

        if (field_name->type != TOKEN_IDENTIFIER)
        {
            free(type);

            parser_set_error(
                state,
                "expected a struct field name."
            );

            parser_free(node);
            return NULL;
        }

        field =
            parser_new_node(
                AST_STRUCT_FIELD,
                field_name
            );

        if (field == NULL)
        {
            free(type);
            parser_free(node);
            return NULL;
        }

        field->struct_field.type = type;

        field->struct_field.pointer_depth =
            pointer_depth;

        field->struct_field.name =
            parser_copy_string(
                field_name->value
            );

        if (field->struct_field.name == NULL)
        {
            parser_free(field);
            parser_free(node);
            return NULL;
        }

        parser_advance(state);

        if (parser_match(
                state,
                TOKEN_LBRACKET
            ))
        {
            if (parser_current(state) != NULL &&
                parser_current(state)->type != TOKEN_RBRACKET)
            {
                field->struct_field.array_length =
                    parser_parse_expression(state);

                if (field->struct_field.array_length == NULL)
                {
                    parser_free(field);
                    parser_free(node);
                    return NULL;
                }
            }

            if (!parser_expect(
                    state,
                    TOKEN_RBRACKET
                ))
            {
                parser_free(field);
                parser_free(node);
                return NULL;
            }
        }

        if (!parser_expect(
                state,
                TOKEN_SEMICOLON
            ))
        {
            parser_free(field);
            parser_free(node);
            return NULL;
        }

        if (!parser_append_ast(
                &node->structure.fields,
                field
            ))
        {
            parser_free(field);
            parser_free(node);
            return NULL;
        }
    }

    if (!parser_expect(
            state,
            TOKEN_SEMICOLON
        ))
    {
        parser_free(node);
        return NULL;
    }

    return node;
}

ast *
parser_parse_function_or_global(
    parser_state *state
)
{
    token *name;
    char *type;
    unsigned long pointer_depth;

    type =
        parser_parse_type(
            state,
            &pointer_depth
        );

    if (type == NULL)
        return NULL;

    name =
        parser_current(state);

    if (name == NULL)
    {
        parser_set_error(
            state,
            "expected a name after type, found end of input."
        );

        free(type);
        return NULL;
    }

    if (name->type != TOKEN_IDENTIFIER)
    {
        parser_set_error(
            state,
            "expected a name after type."
        );

        free(type);
        return NULL;
    }

    parser_advance(state);

    if (parser_match(
            state,
            TOKEN_LPAREN
        ))
    {
        ast *node;

        node =
            parser_new_node(
                AST_FUNCTION,
                name
            );

        if (node == NULL)
        {
            free(type);
            return NULL;
        }

        node->function.return_type =
            type;

        node->function.return_pointer_depth =
            pointer_depth;

        node->function.name =
            parser_copy_string(
                name->value
            );

        if (node->function.name == NULL)
        {
            parser_free(node);
            return NULL;
        }

        if (!parser_match(
                state,
                TOKEN_RPAREN
            ))
        {
            do
            {
                char *parameter_type;
                unsigned long parameter_pointer_depth;
                token *parameter_name;
                ast *parameter;

                parameter_type =
                    parser_parse_type(
                        state,
                        &parameter_pointer_depth
                    );

                if (parameter_type == NULL)
                {
                    parser_free(node);
                    return NULL;
                }

                parameter_name =
                    parser_current(state);

                if (parameter_name == NULL)
                {
                    parser_set_error(
                        state,
                        "expected a parameter name, found end of input."
                    );

                    free(parameter_type);
                    parser_free(node);
                    return NULL;
                }

                if (parameter_name->type !=
                    TOKEN_IDENTIFIER)
                {
                    parser_set_error(
                        state,
                        "expected a parameter name."
                    );

                    free(parameter_type);
                    parser_free(node);
                    return NULL;
                }

                parameter =
                    parser_new_node(
                        AST_PARAMETER,
                        parameter_name
                    );

                if (parameter == NULL)
                {
                    free(parameter_type);
                    parser_free(node);
                    return NULL;
                }

                parameter->parameter.type =
                    parameter_type;

                parameter->parameter.pointer_depth =
                    parameter_pointer_depth;

                parameter->parameter.name =
                    parser_copy_string(
                        parameter_name->value
                    );

                if (parameter->parameter.name == NULL)
                {
                    parser_free(parameter);
                    parser_free(node);
                    return NULL;
                }

                parser_advance(state);

                if (!parser_append_ast(
                        &node->function.parameters,
                        parameter
                    ))
                {
                    parser_free(parameter);
                    parser_free(node);
                    return NULL;
                }

            } while (parser_match(
                state,
                TOKEN_COMMA
            ));

            if (!parser_expect(
                    state,
                    TOKEN_RPAREN
                ))
            {
                parser_free(node);
                return NULL;
            }
        }

        node->function.body =
            parser_parse_block(state);

        if (node->function.body == NULL)
        {
            parser_free(node);
            return NULL;
        }

        return node;
    }

    {
        ast *node;

        node =
            parser_new_node(
                AST_GLOBAL,
                name
            );

        if (node == NULL)
        {
            free(type);
            return NULL;
        }

        node->variable.type = type;

        node->variable.pointer_depth =
            pointer_depth;

        node->variable.name =
            parser_copy_string(
                name->value
            );

        if (node->variable.name == NULL)
        {
            parser_free(node);
            return NULL;
        }

        if (parser_match(
                state,
                TOKEN_LBRACKET
            ))
        {
            if (parser_current(state) != NULL &&
                parser_current(state)->type != TOKEN_RBRACKET)
            {
                node->variable.array_length =
                    parser_parse_expression(state);

                if (node->variable.array_length == NULL)
                {
                    parser_free(node);
                    return NULL;
                }
            }

            if (!parser_expect(
                    state,
                    TOKEN_RBRACKET
                ))
            {
                parser_free(node);
                return NULL;
            }
        }

        if (parser_match(
                state,
                TOKEN_ASSIGN
            ))
        {
            node->variable.value =
                parser_parse_expression(state);

            if (node->variable.value == NULL)
            {
                parser_free(node);
                return NULL;
            }
        }

        if (!parser_expect(
                state,
                TOKEN_SEMICOLON
            ))
        {
            parser_free(node);
            return NULL;
        }

        return node;
    }
}

ast *
parser_parse_block(
    parser_state *state
)
{
    ast *node;

    if (!parser_expect(
            state,
            TOKEN_LBRACE
        ))
        return NULL;

    node =
        parser_new_node(
            AST_BLOCK,
            NULL
        );

    if (node == NULL)
        return NULL;

    while (1)
    {
        ast *statement;

        if (parser_current(state) == NULL)
        {
            parser_set_error(
                state,
                "unexpected end of input inside block."
            );

            parser_free(node);
            return NULL;
        }

        if (parser_current(state)->type ==
            TOKEN_RBRACE)
        {
            parser_advance(state);
            break;
        }

        if (parser_current(state)->type ==
            TOKEN_EOF)
        {
            parser_set_error(
                state,
                "unexpected end of input inside block."
            );

            parser_free(node);
            return NULL;
        }

        statement =
            parser_parse_statement(state);

        if (statement == NULL)
        {
            parser_free(node);
            return NULL;
        }

        if (!parser_append_ast(
                &node->block.statements,
                statement
            ))
        {
            parser_free(statement);
            parser_free(node);
            return NULL;
        }
    }

    return node;
}

ast *
parser_parse_statement(
    parser_state *state
)
{
    token *current;

    current =
        parser_current(state);

    if (current == NULL)
    {
        parser_set_error(
            state,
            "expected a statement, found end of input."
        );

        return NULL;
    }

    if (current->type == TOKEN_LBRACE)
        return parser_parse_block(state);

    if (current->type == TOKEN_IF)
        return parser_parse_if(state);

    if (current->type == TOKEN_FOR)
        return parser_parse_for(state);

    if (current->type == TOKEN_RETURN)
        return parser_parse_return(state);

    if (parser_is_type(current->type))
        return parser_parse_variable_declaration(state);

    return parser_parse_expression_statement(state);
}

ast *
parser_parse_variable_declaration(
    parser_state *state
)
{
    ast *node;
    token *name;
    char *type;
    unsigned long pointer_depth;

    type =
        parser_parse_type(
            state,
            &pointer_depth
        );

    if (type == NULL)
        return NULL;

    name =
        parser_current(state);

    if (name == NULL)
    {
        parser_set_error(
            state,
            "expected a variable name, found end of input."
        );

        free(type);
        return NULL;
    }

    if (name->type != TOKEN_IDENTIFIER)
    {
        parser_set_error(
            state,
            "expected a variable name."
        );

        free(type);
        return NULL;
    }

    node =
        parser_new_node(
            AST_VARIABLE_DECLARATION,
            name
        );

    if (node == NULL)
    {
        free(type);
        return NULL;
    }

    node->variable.type = type;

    node->variable.pointer_depth =
        pointer_depth;

    node->variable.name =
        parser_copy_string(
            name->value
        );

    if (node->variable.name == NULL)
    {
        parser_free(node);
        return NULL;
    }

    parser_advance(state);

    if (parser_match(
            state,
            TOKEN_LBRACKET
        ))
    {
        if (parser_current(state) != NULL &&
            parser_current(state)->type != TOKEN_RBRACKET)
        {
            node->variable.array_length =
                parser_parse_expression(state);

            if (node->variable.array_length == NULL)
            {
                parser_free(node);
                return NULL;
            }
        }

        if (!parser_expect(
                state,
                TOKEN_RBRACKET
            ))
        {
            parser_free(node);
            return NULL;
        }
    }

    if (parser_match(
            state,
            TOKEN_ASSIGN
        ))
    {
        node->variable.value =
            parser_parse_expression(state);

        if (node->variable.value == NULL)
        {
            parser_free(node);
            return NULL;
        }
    }

    if (!parser_expect(
            state,
            TOKEN_SEMICOLON
        ))
    {
        parser_free(node);
        return NULL;
    }

    return node;
}

ast *
parser_parse_if(
    parser_state *state
)
{
    ast *node;

    node =
        parser_new_node(
            AST_IF,
            parser_current(state)
        );

    if (node == NULL)
        return NULL;

    parser_advance(state);

    if (!parser_expect(
            state,
            TOKEN_LPAREN
        ))
    {
        parser_free(node);
        return NULL;
    }

    node->if_statement.condition =
        parser_parse_expression(state);

    if (node->if_statement.condition == NULL)
    {
        parser_free(node);
        return NULL;
    }

    if (!parser_expect(
            state,
            TOKEN_RPAREN
        ))
    {
        parser_free(node);
        return NULL;
    }

    node->if_statement.then_body =
        parser_parse_statement(state);

    if (node->if_statement.then_body == NULL)
    {
        parser_free(node);
        return NULL;
    }

    if (parser_match(
            state,
            TOKEN_ELSE
        ))
    {
        node->if_statement.else_body =
            parser_parse_statement(state);

        if (node->if_statement.else_body == NULL)
        {
            parser_free(node);
            return NULL;
        }
    }

    return node;
}

ast *
parser_parse_for(
    parser_state *state
)
{
    ast *node;

    node =
        parser_new_node(
            AST_FOR,
            parser_current(state)
        );

    if (node == NULL)
        return NULL;

    parser_advance(state);

    if (!parser_expect(
            state,
            TOKEN_LPAREN
        ))
    {
        parser_free(node);
        return NULL;
    }

    if (!parser_match(
            state,
            TOKEN_SEMICOLON
        ))
    {
        token *current;

        current =
            parser_current(state);

        if (current == NULL)
        {
            parser_set_error(
                state,
                "expected a for-loop initializer."
            );

            parser_free(node);
            return NULL;
        }

        if (parser_is_type(
                current->type))
        {
            node->for_statement.init =
                parser_parse_variable_declaration(
                    state
                );
        }
        else
        {
            node->for_statement.init =
                parser_parse_expression_statement(
                    state
                );
        }

        if (node->for_statement.init == NULL)
        {
            parser_free(node);
            return NULL;
        }
    }

    if (!parser_match(
            state,
            TOKEN_SEMICOLON
        ))
    {
        node->for_statement.condition =
            parser_parse_expression(state);

        if (node->for_statement.condition == NULL)
        {
            parser_free(node);
            return NULL;
        }

        if (!parser_expect(
                state,
                TOKEN_SEMICOLON
            ))
        {
            parser_free(node);
            return NULL;
        }
    }

    if (!parser_match(
            state,
            TOKEN_RPAREN
        ))
    {
        node->for_statement.increment =
            parser_parse_expression(state);

        if (node->for_statement.increment == NULL)
        {
            parser_free(node);
            return NULL;
        }

        if (!parser_expect(
                state,
                TOKEN_RPAREN
            ))
        {
            parser_free(node);
            return NULL;
        }
    }

    node->for_statement.body =
        parser_parse_statement(state);

    if (node->for_statement.body == NULL)
    {
        parser_free(node);
        return NULL;
    }

    return node;
}

ast *
parser_parse_return(
    parser_state *state
)
{
    ast *node;

    node =
        parser_new_node(
            AST_RETURN,
            parser_current(state)
        );

    if (node == NULL)
        return NULL;

    parser_advance(state);

    if (!parser_match(
            state,
            TOKEN_SEMICOLON
        ))
    {
        node->return_statement.value =
            parser_parse_expression(state);

        if (node->return_statement.value == NULL)
        {
            parser_free(node);
            return NULL;
        }

        if (!parser_expect(
                state,
                TOKEN_SEMICOLON
            ))
        {
            parser_free(node);
            return NULL;
        }
    }

    return node;
}

ast *
parser_parse_expression_statement(
    parser_state *state
)
{
    ast *node;
    ast *expression;

    node =
        parser_new_node(
            AST_EXPRESSION_STATEMENT,
            parser_current(state)
        );

    if (node == NULL)
        return NULL;

    expression =
        parser_parse_expression(state);

    if (expression == NULL)
    {
        parser_free(node);
        return NULL;
    }

    node->expression_statement.expression =
        expression;

    if (!parser_expect(
            state,
            TOKEN_SEMICOLON
        ))
    {
        parser_free(node);
        return NULL;
    }

    return node;
}

ast *
parser_parse_expression(
    parser_state *state
)
{
    return parser_parse_assignment(state);
}

ast *
parser_parse_assignment(
    parser_state *state
)
{
    ast *left;
    ast *right;
    ast *node;

    left =
        parser_parse_logical_or(state);

    if (left == NULL)
        return NULL;

    if (!parser_match(
            state,
            TOKEN_ASSIGN
        ))
        return left;

    right =
        parser_parse_assignment(state);

    if (right == NULL)
    {
        parser_free(left);
        return NULL;
    }

    node =
        parser_new_node(
            AST_ASSIGNMENT,
            NULL
        );

    if (node == NULL)
    {
        parser_free(left);
        parser_free(right);
        return NULL;
    }

    node->assignment.left = left;
    node->assignment.right = right;

    return node;
}

ast *
parser_parse_logical_or(
    parser_state *state
)
{
    ast *left;

    left =
        parser_parse_logical_and(state);

    if (left == NULL)
        return NULL;

    while (parser_match(
        state,
        TOKEN_OR
    ))
    {
        ast *right;
        ast *node;

        right =
            parser_parse_logical_and(state);

        if (right == NULL)
        {
            parser_free(left);
            return NULL;
        }

        node =
            parser_new_node(
                AST_BINARY,
                NULL
            );

        if (node == NULL)
        {
            parser_free(left);
            parser_free(right);
            return NULL;
        }

        node->binary.operator =
            AST_BINARY_OR;

        node->binary.left = left;
        node->binary.right = right;

        left = node;
    }

    return left;
}

ast *
parser_parse_logical_and(
    parser_state *state
)
{
    ast *left;

    left =
        parser_parse_equality(state);

    if (left == NULL)
        return NULL;

    while (parser_match(
        state,
        TOKEN_AND
    ))
    {
        ast *right;
        ast *node;

        right =
            parser_parse_equality(state);

        if (right == NULL)
        {
            parser_free(left);
            return NULL;
        }

        node =
            parser_new_node(
                AST_BINARY,
                NULL
            );

        if (node == NULL)
        {
            parser_free(left);
            parser_free(right);
            return NULL;
        }

        node->binary.operator =
            AST_BINARY_AND;

        node->binary.left = left;
        node->binary.right = right;

        left = node;
    }

    return left;
}

ast *
parser_parse_equality(
    parser_state *state
)
{
    ast *left;

    left =
        parser_parse_comparison(state);

    if (left == NULL)
        return NULL;

    while (
        parser_current(state) != NULL &&
        (
            parser_current(state)->type ==
                TOKEN_EQUAL ||
            parser_current(state)->type ==
                TOKEN_NOT_EQUAL
        )
    )
    {
        token_type operator;
        ast *right;
        ast *node;

        operator =
            parser_advance(state)->type;

        right =
            parser_parse_comparison(state);

        if (right == NULL)
        {
            parser_free(left);
            return NULL;
        }

        node =
            parser_new_node(
                AST_BINARY,
                NULL
            );

        if (node == NULL)
        {
            parser_free(left);
            parser_free(right);
            return NULL;
        }

        node->binary.operator =
            operator == TOKEN_EQUAL
                ? AST_BINARY_EQUAL
                : AST_BINARY_NOT_EQUAL;

        node->binary.left = left;
        node->binary.right = right;

        left = node;
    }

    return left;
}

ast *
parser_parse_comparison(
    parser_state *state
)
{
    ast *left;

    left =
        parser_parse_term(state);

    if (left == NULL)
        return NULL;

    while (parser_current(state) != NULL)
    {
        token_type type;
        ast_binary_operator operator;
        ast *right;
        ast *node;

        type =
            parser_current(state)->type;

        if (type == TOKEN_LESS)
            operator = AST_BINARY_LESS;
        else if (type == TOKEN_LESS_EQUAL)
            operator = AST_BINARY_LESS_EQUAL;
        else if (type == TOKEN_GREATER)
            operator = AST_BINARY_GREATER;
        else if (type == TOKEN_GREATER_EQUAL)
            operator = AST_BINARY_GREATER_EQUAL;
        else
            break;

        parser_advance(state);

        right =
            parser_parse_term(state);

        if (right == NULL)
        {
            parser_free(left);
            return NULL;
        }

        node =
            parser_new_node(
                AST_BINARY,
                NULL
            );

        if (node == NULL)
        {
            parser_free(left);
            parser_free(right);
            return NULL;
        }

        node->binary.operator = operator;
        node->binary.left = left;
        node->binary.right = right;

        left = node;
    }

    return left;
}

ast *
parser_parse_term(
    parser_state *state
)
{
    ast *left;

    left =
        parser_parse_factor(state);

    if (left == NULL)
        return NULL;

    while (
        parser_current(state) != NULL &&
        (
            parser_current(state)->type ==
                TOKEN_PLUS ||
            parser_current(state)->type ==
                TOKEN_MINUS
        )
    )
    {
        token_type operator;
        ast *right;
        ast *node;

        operator =
            parser_advance(state)->type;

        right =
            parser_parse_factor(state);

        if (right == NULL)
        {
            parser_free(left);
            return NULL;
        }

        node =
            parser_new_node(
                AST_BINARY,
                NULL
            );

        if (node == NULL)
        {
            parser_free(left);
            parser_free(right);
            return NULL;
        }

        node->binary.operator =
            operator == TOKEN_PLUS
                ? AST_BINARY_ADD
                : AST_BINARY_SUBTRACT;

        node->binary.left = left;
        node->binary.right = right;

        left = node;
    }

    return left;
}

ast *
parser_parse_factor(
    parser_state *state
)
{
    ast *left;

    left =
        parser_parse_unary(state);

    if (left == NULL)
        return NULL;

    while (
        parser_current(state) != NULL &&
        (
            parser_current(state)->type ==
                TOKEN_STAR ||
            parser_current(state)->type ==
                TOKEN_SLASH ||
            parser_current(state)->type ==
                TOKEN_PERCENT
        )
    )
    {
        token_type operator;
        ast *right;
        ast *node;

        operator =
            parser_advance(state)->type;

        right =
            parser_parse_unary(state);

        if (right == NULL)
        {
            parser_free(left);
            return NULL;
        }

        node =
            parser_new_node(
                AST_BINARY,
                NULL
            );

        if (node == NULL)
        {
            parser_free(left);
            parser_free(right);
            return NULL;
        }

        if (operator == TOKEN_STAR)
        {
            node->binary.operator =
                AST_BINARY_MULTIPLY;
        }
        else if (operator == TOKEN_SLASH)
        {
            node->binary.operator =
                AST_BINARY_DIVIDE;
        }
        else
        {
            node->binary.operator =
                AST_BINARY_MODULO;
        }

        node->binary.left = left;
        node->binary.right = right;

        left = node;
    }

    return left;
}

ast *
parser_parse_unary(
    parser_state *state
)
{
    token *current;
    ast *operand;
    ast *node;
    ast_unary_operator operator;

    current =
        parser_current(state);

    if (current == NULL)
    {
        parser_set_error(
            state,
            "expected an expression, found end of input."
        );

        return NULL;
    }

    if (
        current->type == TOKEN_PLUS ||
        current->type == TOKEN_MINUS ||
        current->type == TOKEN_NOT ||
        current->type == TOKEN_ADDRESS ||
        current->type == TOKEN_STAR
    )
    {
        if (current->type == TOKEN_PLUS)
            operator = AST_UNARY_PLUS;
        else if (current->type == TOKEN_MINUS)
            operator = AST_UNARY_MINUS;
        else if (current->type == TOKEN_NOT)
            operator = AST_UNARY_NOT;
        else if (current->type == TOKEN_ADDRESS)
            operator = AST_UNARY_ADDRESS;
        else
            operator = AST_UNARY_DEREFERENCE;

        parser_advance(state);

        operand =
            parser_parse_unary(state);

        if (operand == NULL)
            return NULL;

        node =
            parser_new_node(
                AST_UNARY,
                current
            );

        if (node == NULL)
        {
            parser_free(operand);
            return NULL;
        }

        node->unary.operator = operator;
        node->unary.operand = operand;

        return node;
    }

    return parser_parse_postfix(state);
}

ast *
parser_parse_postfix(
    parser_state *state
)
{
    ast *node;

    node =
        parser_parse_primary(state);

    if (node == NULL)
        return NULL;

    while (parser_current(state) != NULL)
    {
        if (parser_match(
                state,
                TOKEN_LPAREN
            ))
        {
            ast *call;

            call =
                parser_new_node(
                    AST_CALL,
                    NULL
                );

            if (call == NULL)
            {
                parser_free(node);
                return NULL;
            }

            call->call.function = node;

            if (!parser_match(
                    state,
                    TOKEN_RPAREN
                ))
            {
                while (1)
                {
                    ast *argument;

                    argument =
                        parser_parse_expression(state);

                    if (argument == NULL)
                    {
                        parser_free(call);
                        return NULL;
                    }

                    if (!parser_append_ast(
                            &call->call.arguments,
                            argument
                        ))
                    {
                        parser_free(argument);
                        parser_free(call);
                        return NULL;
                    }

                    if (!parser_match(
                            state,
                            TOKEN_COMMA
                        ))
                        break;

                    if (
                        parser_current(state) == NULL ||
                        parser_current(state)->type ==
                            TOKEN_RPAREN
                    )
                    {
                        parser_set_error(
                            state,
                            "expected an expression after ','."
                        );

                        parser_free(call);
                        return NULL;
                    }
                }

                if (!parser_expect(
                        state,
                        TOKEN_RPAREN
                    ))
                {
                    parser_free(call);
                    return NULL;
                }
            }

            node = call;
            continue;
        }

        if (parser_match(
                state,
                TOKEN_LBRACKET
            ))
        {
            ast *index;
            ast *index_expression;

            index_expression =
                parser_parse_expression(state);

            if (index_expression == NULL)
            {
                parser_free(node);
                return NULL;
            }

            if (!parser_expect(
                    state,
                    TOKEN_RBRACKET
                ))
            {
                parser_free(index_expression);
                parser_free(node);
                return NULL;
            }

            index =
                parser_new_node(
                    AST_INDEX,
                    NULL
                );

            if (index == NULL)
            {
                parser_free(index_expression);
                parser_free(node);
                return NULL;
            }

            index->index.object = node;
            index->index.index = index_expression;

            node = index;
            continue;
        }

        if (parser_match(
                state,
                TOKEN_DOT
            ))
        {
            token *name;
            ast *member;

            name =
                parser_current(state);

            if (name == NULL)
            {
                parser_set_error(
                    state,
                    "expected a member name, found end of input."
                );

                parser_free(node);
                return NULL;
            }

            if (name->type != TOKEN_IDENTIFIER)
            {
                parser_set_error(
                    state,
                    "expected a member name."
                );

                parser_free(node);
                return NULL;
            }

            member =
                parser_new_node(
                    AST_MEMBER,
                    name
                );

            if (member == NULL)
            {
                parser_free(node);
                return NULL;
            }

            member->member.object = node;

            member->member.name =
                parser_copy_string(
                    name->value
                );

            if (member->member.name == NULL)
            {
                parser_free(member);
                return NULL;
            }

            parser_advance(state);

            node = member;
            continue;
        }

        if (parser_match(
                state,
                TOKEN_ARROW
            ))
        {
            token *name;
            ast *member;

            name =
                parser_current(state);

            if (name == NULL)
            {
                parser_set_error(
                    state,
                    "expected a member name, found end of input."
                );

                parser_free(node);
                return NULL;
            }

            if (name->type != TOKEN_IDENTIFIER)
            {
                parser_set_error(
                    state,
                    "expected a member name."
                );

                parser_free(node);
                return NULL;
            }

            member =
                parser_new_node(
                    AST_POINTER_MEMBER,
                    name
                );

            if (member == NULL)
            {
                parser_free(node);
                return NULL;
            }

            member->pointer_member.object =
                node;

            member->pointer_member.name =
                parser_copy_string(
                    name->value
                );

            if (member->pointer_member.name == NULL)
            {
                parser_free(member);
                return NULL;
            }

            parser_advance(state);

            node = member;
            continue;
        }

        break;
    }

    return node;
}

ast *
parser_parse_primary(
    parser_state *state
)
{
    token *current;
    ast *node;

    current =
        parser_current(state);

    if (current == NULL)
    {
        parser_set_error(
            state,
            "expected an expression, found end of input."
        );

        return NULL;
    }

    if (current->type == TOKEN_INTEGER)
    {
        node =
            parser_new_node(
                AST_INTEGER,
                current
            );

        if (node == NULL)
            return NULL;

        node->integer.value =
            parser_copy_string(
                current->value
            );

        if (node->integer.value == NULL)
        {
            parser_free(node);
            return NULL;
        }

        parser_advance(state);

        return node;
    }

    if (current->type == TOKEN_IDENTIFIER)
    {
        node =
            parser_new_node(
                AST_IDENTIFIER,
                current
            );

        if (node == NULL)
            return NULL;

        node->identifier.name =
            parser_copy_string(
                current->value
            );

        if (node->identifier.name == NULL)
        {
            parser_free(node);
            return NULL;
        }

        parser_advance(state);

        return node;
    }

    /*
     * Parenthesized expression:
     *
     *     ( expression )
     */
    if (parser_match(
            state,
            TOKEN_LPAREN
        ))
    {
        node =
            parser_parse_expression(state);

        if (node == NULL)
            return NULL;

        if (!parser_expect(
                state,
                TOKEN_RPAREN
            ))
        {
            parser_free(node);
            return NULL;
        }

        return node;
    }

    if (current->type == TOKEN_EOF)
    {
        parser_set_error(
            state,
            "expected an expression, found end of input."
        );
    }
    else if (current->value != NULL)
    {
        char message[256];

        snprintf(
            message,
            sizeof(message),
            "expected an expression, found '%s'.",
            current->value
        );

        parser_set_error(
            state,
            message
        );
    }
    else
    {
        parser_set_error(
            state,
            "expected an expression."
        );
    }

    return NULL;
}

void
parser_free_list(
    ast_list *list
)
{
    unsigned long i;

    if (list == NULL)
        return;

    for (i = 0; i < list->count; i++)
        parser_free(list->values[i]);

    free(list->values);

    list->values = NULL;
    list->count = 0;
}

void
parser_free(
    ast *node
)
{
    if (node == NULL)
        return;

    free(node->file);

    switch (node->type)
    {
        case AST_PROGRAM:
            parser_free_list(
                &node->program.imports
            );

            parser_free_list(
                &node->program.structs
            );

            parser_free_list(
                &node->program.globals
            );

            parser_free_list(
                &node->program.functions
            );

            break;

        case AST_IMPORT:
            free(node->import.path);

            parser_free_list(
                &node->import.names
            );

            break;

        case AST_STRUCT:
            free(node->structure.name);

            parser_free_list(
                &node->structure.fields
            );

            break;

        case AST_STRUCT_FIELD:
            free(node->struct_field.type);
            free(node->struct_field.name);

            parser_free(
                node->struct_field.array_length
            );

            break;

        case AST_GLOBAL:
        case AST_VARIABLE_DECLARATION:
            free(node->variable.type);
            free(node->variable.name);

            parser_free(
                node->variable.value
            );

            parser_free(
                node->variable.array_length
            );

            break;

        case AST_EXPRESSION_STATEMENT:
            parser_free(
                node->expression_statement.expression
            );

            break;

        case AST_FUNCTION:
            free(node->function.return_type);
            free(node->function.name);

            parser_free_list(
                &node->function.parameters
            );

            parser_free(
                node->function.body
            );

            break;

        case AST_PARAMETER:
            free(node->parameter.type);
            free(node->parameter.name);
            break;

        case AST_BLOCK:
            parser_free_list(
                &node->block.statements
            );

            break;

        case AST_IF:
            parser_free(
                node->if_statement.condition
            );

            parser_free(
                node->if_statement.then_body
            );

            parser_free(
                node->if_statement.else_body
            );

            break;

        case AST_FOR:
            parser_free(
                node->for_statement.init
            );

            parser_free(
                node->for_statement.condition
            );

            parser_free(
                node->for_statement.increment
            );

            parser_free(
                node->for_statement.body
            );

            break;

        case AST_RETURN:
            parser_free(
                node->return_statement.value
            );

            break;

        case AST_ASSIGNMENT:
            parser_free(
                node->assignment.left
            );

            parser_free(
                node->assignment.right
            );

            break;

        case AST_INTEGER:
            free(node->integer.value);
            break;

        case AST_IDENTIFIER:
            free(node->identifier.name);
            break;

        case AST_UNARY:
            parser_free(
                node->unary.operand
            );

            break;

        case AST_BINARY:
            parser_free(
                node->binary.left
            );

            parser_free(
                node->binary.right
            );

            break;

        case AST_CALL:
            parser_free(
                node->call.function
            );

            parser_free_list(
                &node->call.arguments
            );

            break;

        case AST_INDEX:
            parser_free(
                node->index.object
            );

            parser_free(
                node->index.index
            );

            break;

        case AST_MEMBER:
            parser_free(
                node->member.object
            );

            free(node->member.name);

            break;

        case AST_POINTER_MEMBER:
            parser_free(
                node->pointer_member.object
            );

            free(node->pointer_member.name);

            break;
    }

    free(node);
}
