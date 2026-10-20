#include <stdio.h>
#include <stdlib.h>

#include "internal.h"

parser_result
parser(
    lexer_result lexer
)
{
    parser_result result;
    parser_state state;
    token_stream combined;
    token *current;
    unsigned long combined_count;
    unsigned long combined_position;

    result.error = 0;
    result.error_text = NULL;
    result.file = NULL;
    result.line = 0;
    result.column = 0;
    result.tree = NULL;

    state.stream = NULL;
    state.position = 0;
    state.count = 0;
    state.error_text = NULL;
    state.error_token = NULL;
    combined.values = NULL;
    combined.count = 0;

    /*
     * The lexer must succeed before the parser
     * can process the token stream.
     */
    if (lexer.error)
    {
        result.error = 1;

        result.error_text =
            parser_copy_string(
                "cannot parse input because lexer failed."
            );

        return result;
    }

    /*
     * A lexer result without any token streams
     * is not valid parser input.
     */
    if (lexer.count == 0)
    {
        result.error = 1;

        result.error_text =
            parser_copy_string(
                "no token streams were provided."
            );

        return result;
    }

    combined_count = 1;
    for (unsigned long stream_index = 0;
         stream_index < lexer.count;
         stream_index++)
    {
        if (lexer.streams[stream_index].count == 0) {
            result.error = 1;
            result.error_text = parser_copy_string(
                "empty token stream was provided."
            );
            return result;
        }
        combined_count += lexer.streams[stream_index].count - 1;
    }

    combined.values = calloc(combined_count, sizeof(token));
    if (!combined.values) {
        result.error = 1;
        result.error_text = parser_copy_string(
            "failed to allocate combined token stream."
        );
        return result;
    }

    combined_position = 0;
    for (unsigned long stream_index = 0;
         stream_index < lexer.count;
         stream_index++)
    {
        token_stream *stream = &lexer.streams[stream_index];

        for (unsigned long token_index = 0;
             token_index + 1 < stream->count;
             token_index++)
        {
            combined.values[combined_position++] =
                stream->values[token_index];
        }
    }

    combined.values[combined_position++] =
        lexer.streams[lexer.count - 1].values[
            lexer.streams[lexer.count - 1].count - 1
        ];
    combined.count = combined_position;

    state.stream = &combined;

    state.position = 0;

    state.count = combined.count;

    /*
     * Parse the complete program.
     */
    result.tree =
        parser_parse_program(
            &state
        );

    /*
     * Parsing failed.
     *
     * Use the first concrete syntax error
     * recorded by the parser.
     */
    if (result.tree == NULL)
    {
        result.error = 1;

        if (state.error_text != NULL)
        {
            result.error_text =
                state.error_text;

            state.error_text = NULL;
        }
        else
        {
            result.error_text =
                parser_copy_string(
                    "unexpected token."
                );
        }

        /*
         * parser_set_error() records the token
         * that caused the first syntax error.
         */
        current =
            state.error_token;

        if (current == NULL)
            current = parser_current(&state);

        if (current != NULL)
        {
            result.file =
                parser_copy_string(
                    current->file
                );

            result.line =
                current->line;

            result.column =
                current->column;
        }

        free(state.error_text);
        free(combined.values);

        return result;
    }

    free(state.error_text);
    free(combined.values);

    return result;
}

void
parser_handle_error(
    parser_result result
)
{
    if (!result.error)
        return;

    fprintf(
        stderr,
        "parser error: %s\n",
        result.error_text != NULL
            ? result.error_text
            : "unknown parser error."
    );

    if (result.file != NULL)
    {
        fprintf(
            stderr,
            "  --> %s:%lu:%lu\n",
            result.file,
            result.line,
            result.column
        );
    }

    fprintf(
        stderr,
        "\nfor further support visit README.md or --help.\n"
    );
}

static void
parser_print_indent(
    unsigned long depth
)
{
    unsigned long i;

    for (i = 0; i < depth; i++)
        printf("  ");
}

static const char *
parser_unary_operator_name(
    ast_unary_operator operator
)
{
    switch (operator)
    {
        case AST_UNARY_PLUS:
            return "PLUS";

        case AST_UNARY_MINUS:
            return "MINUS";

        case AST_UNARY_NOT:
            return "NOT";

        case AST_UNARY_ADDRESS:
            return "ADDRESS";

        case AST_UNARY_DEREFERENCE:
            return "DEREFERENCE";
    }

    return "UNKNOWN";
}

static const char *
parser_binary_operator_name(
    ast_binary_operator operator
)
{
    switch (operator)
    {
        case AST_BINARY_ADD:
            return "ADD";

        case AST_BINARY_SUBTRACT:
            return "SUBTRACT";

        case AST_BINARY_MULTIPLY:
            return "MULTIPLY";

        case AST_BINARY_DIVIDE:
            return "DIVIDE";

        case AST_BINARY_MODULO:
            return "MODULO";

        case AST_BINARY_EQUAL:
            return "EQUAL";

        case AST_BINARY_NOT_EQUAL:
            return "NOT_EQUAL";

        case AST_BINARY_LESS:
            return "LESS";

        case AST_BINARY_LESS_EQUAL:
            return "LESS_EQUAL";

        case AST_BINARY_GREATER:
            return "GREATER";

        case AST_BINARY_GREATER_EQUAL:
            return "GREATER_EQUAL";

        case AST_BINARY_AND:
            return "AND";

        case AST_BINARY_OR:
            return "OR";
    }

    return "UNKNOWN";
}

static void
parser_print_list(
    ast_list *list,
    unsigned long depth
);

static void
parser_print_ast_node(
    ast *tree,
    unsigned long depth
)
{
    if (tree == NULL)
    {
        parser_print_indent(depth);
        printf("(null)\n");
        return;
    }

    parser_print_indent(depth);

    switch (tree->type)
    {
        case AST_PROGRAM:
            printf("PROGRAM\n");

            parser_print_indent(depth + 1);
            printf("imports:\n");

            parser_print_list(
                &tree->program.imports,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("structs:\n");

            parser_print_list(
                &tree->program.structs,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("globals:\n");

            parser_print_list(
                &tree->program.globals,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("functions:\n");

            parser_print_list(
                &tree->program.functions,
                depth + 2
            );

            break;

        case AST_IMPORT:
            printf("IMPORT\n");

            parser_print_indent(depth + 1);
            printf(
                "path: %s\n",
                tree->import.path != NULL
                    ? tree->import.path
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf("names:\n");

            parser_print_list(
                &tree->import.names,
                depth + 2
            );

            break;

        case AST_STRUCT:
            printf("STRUCT\n");

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->structure.name != NULL
                    ? tree->structure.name
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf("fields:\n");

            parser_print_list(
                &tree->structure.fields,
                depth + 2
            );

            break;

        case AST_STRUCT_FIELD:
            printf("STRUCT_FIELD\n");

            parser_print_indent(depth + 1);
            printf(
                "type: %s\n",
                tree->struct_field.type != NULL
                    ? tree->struct_field.type
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->struct_field.name != NULL
                    ? tree->struct_field.name
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "pointer_depth: %lu\n",
                tree->struct_field.pointer_depth
            );

            break;

        case AST_GLOBAL:
            printf("GLOBAL\n");

            parser_print_indent(depth + 1);
            printf(
                "type: %s\n",
                tree->variable.type != NULL
                    ? tree->variable.type
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->variable.name != NULL
                    ? tree->variable.name
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "pointer_depth: %lu\n",
                tree->variable.pointer_depth
            );

            parser_print_indent(depth + 1);
            printf("value:\n");

            parser_print_ast_node(
                tree->variable.value,
                depth + 2
            );

            break;

        case AST_FUNCTION:
            printf("FUNCTION\n");

            parser_print_indent(depth + 1);
            printf(
                "return_type: %s\n",
                tree->function.return_type != NULL
                    ? tree->function.return_type
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "return_pointer_depth: %lu\n",
                tree->function.return_pointer_depth
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->function.name != NULL
                    ? tree->function.name
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf("parameters:\n");

            parser_print_list(
                &tree->function.parameters,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("body:\n");

            parser_print_ast_node(
                tree->function.body,
                depth + 2
            );

            break;

        case AST_PARAMETER:
            printf("PARAMETER\n");

            parser_print_indent(depth + 1);
            printf(
                "type: %s\n",
                tree->parameter.type != NULL
                    ? tree->parameter.type
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->parameter.name != NULL
                    ? tree->parameter.name
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "pointer_depth: %lu\n",
                tree->parameter.pointer_depth
            );

            break;

        case AST_BLOCK:
            printf("BLOCK\n");

            parser_print_indent(depth + 1);
            printf("statements:\n");

            parser_print_list(
                &tree->block.statements,
                depth + 2
            );

            break;

        case AST_VARIABLE_DECLARATION:
            printf("VARIABLE_DECLARATION\n");

            parser_print_indent(depth + 1);
            printf(
                "type: %s\n",
                tree->variable.type != NULL
                    ? tree->variable.type
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->variable.name != NULL
                    ? tree->variable.name
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf(
                "pointer_depth: %lu\n",
                tree->variable.pointer_depth
            );

            parser_print_indent(depth + 1);
            printf("value:\n");

            parser_print_ast_node(
                tree->variable.value,
                depth + 2
            );

            break;

        case AST_EXPRESSION_STATEMENT:
            printf("EXPRESSION_STATEMENT\n");

            parser_print_indent(depth + 1);
            printf("expression:\n");

            parser_print_ast_node(
                tree->expression_statement.expression,
                depth + 2
            );

            break;

        case AST_IF:
            printf("IF\n");

            parser_print_indent(depth + 1);
            printf("condition:\n");

            parser_print_ast_node(
                tree->if_statement.condition,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("then_body:\n");

            parser_print_ast_node(
                tree->if_statement.then_body,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("else_body:\n");

            parser_print_ast_node(
                tree->if_statement.else_body,
                depth + 2
            );

            break;

        case AST_FOR:
            printf("FOR\n");

            parser_print_indent(depth + 1);
            printf("init:\n");

            parser_print_ast_node(
                tree->for_statement.init,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("condition:\n");

            parser_print_ast_node(
                tree->for_statement.condition,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("increment:\n");

            parser_print_ast_node(
                tree->for_statement.increment,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("body:\n");

            parser_print_ast_node(
                tree->for_statement.body,
                depth + 2
            );

            break;

        case AST_RETURN:
            printf("RETURN\n");

            parser_print_indent(depth + 1);
            printf("value:\n");

            parser_print_ast_node(
                tree->return_statement.value,
                depth + 2
            );

            break;

        case AST_ASSIGNMENT:
            printf("ASSIGNMENT\n");

            parser_print_indent(depth + 1);
            printf("left:\n");

            parser_print_ast_node(
                tree->assignment.left,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("right:\n");

            parser_print_ast_node(
                tree->assignment.right,
                depth + 2
            );

            break;

        case AST_ASM:
            printf("ASM\n");

            parser_print_indent(depth + 1);
            printf(
                "template: %s\n",
                tree->assembly.template != NULL
                    ? tree->assembly.template
                    : "(null)"
            );

            parser_print_indent(depth + 1);
            printf("outputs:\n");
            parser_print_list(&tree->assembly.outputs, depth + 2);

            parser_print_indent(depth + 1);
            printf("inputs:\n");
            parser_print_list(&tree->assembly.inputs, depth + 2);

            parser_print_indent(depth + 1);
            printf("clobbers:\n");
            parser_print_list(&tree->assembly.clobbers, depth + 2);
            break;

        case AST_ASM_OPERAND:
            printf(
                "ASM_OPERAND: %s\n",
                tree->assembly_operand.constraint != NULL
                    ? tree->assembly_operand.constraint
                    : "(null)"
            );
            parser_print_ast_node(
                tree->assembly_operand.expression,
                depth + 1
            );
            break;

        case AST_INTEGER:
            printf(
                "INTEGER: %s\n",
                tree->integer.value != NULL
                    ? tree->integer.value
                    : "(null)"
            );

            break;

        case AST_STRING:
            printf(
                "STRING: %s\n",
                tree->string.value != NULL
                    ? tree->string.value
                    : "(null)"
            );

            break;

        case AST_IDENTIFIER:
            printf(
                "IDENTIFIER: %s\n",
                tree->identifier.name != NULL
                    ? tree->identifier.name
                    : "(null)"
            );

            break;

        case AST_UNARY:
            printf(
                "UNARY: %s\n",
                parser_unary_operator_name(
                    tree->unary.operator
                )
            );

            parser_print_indent(depth + 1);
            printf("operand:\n");

            parser_print_ast_node(
                tree->unary.operand,
                depth + 2
            );

            break;

        case AST_BINARY:
            printf(
                "BINARY: %s\n",
                parser_binary_operator_name(
                    tree->binary.operator
                )
            );

            parser_print_indent(depth + 1);
            printf("left:\n");

            parser_print_ast_node(
                tree->binary.left,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("right:\n");

            parser_print_ast_node(
                tree->binary.right,
                depth + 2
            );

            break;

        case AST_CALL:
            printf("CALL\n");

            parser_print_indent(depth + 1);
            printf("function:\n");

            parser_print_ast_node(
                tree->call.function,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("arguments:\n");

            parser_print_list(
                &tree->call.arguments,
                depth + 2
            );

            break;

        case AST_INDEX:
            printf("INDEX\n");

            parser_print_indent(depth + 1);
            printf("object:\n");

            parser_print_ast_node(
                tree->index.object,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf("index:\n");

            parser_print_ast_node(
                tree->index.index,
                depth + 2
            );

            break;

        case AST_MEMBER:
            printf("MEMBER\n");

            parser_print_indent(depth + 1);
            printf("object:\n");

            parser_print_ast_node(
                tree->member.object,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->member.name != NULL
                    ? tree->member.name
                    : "(null)"
            );

            break;

        case AST_POINTER_MEMBER:
            printf("POINTER_MEMBER\n");

            parser_print_indent(depth + 1);
            printf("object:\n");

            parser_print_ast_node(
                tree->pointer_member.object,
                depth + 2
            );

            parser_print_indent(depth + 1);
            printf(
                "name: %s\n",
                tree->pointer_member.name != NULL
                    ? tree->pointer_member.name
                    : "(null)"
            );

            break;
    }
}

static void
parser_print_list(
    ast_list *list,
    unsigned long depth
)
{
    unsigned long i;

    if (list == NULL)
        return;

    for (i = 0; i < list->count; i++)
    {
        parser_print_ast_node(
            list->values[i],
            depth
        );
    }
}

void
parser_print_ast(
    ast *tree
)
{
    parser_print_ast_node(
        tree,
        0
    );
}
