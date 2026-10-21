/**
 * @file main.c
 * @brief LowC compiler entry point.
 */

#include <stdio.h>

#include "main_arg_handler/public.h"
#include "flag_handler/public.h"
#include "files_handler/public.h"
#include "lexer/public.h"
#include "parser/public.h"
#include "semantic/public.h"
#include "ir/public.h"
#include "codegen/public.h"
#include "output/public.h"
#include "formatter/public.h"


int
main(
    int argc,
    char **argv
)
{
    flags *flags;
    files *files;

    main_arg_handler_result args =
        main_arg_handler(
            argc,
            argv,
            &flags,
            &files
        );

    if (args.error)
    {
        main_arg_handler_handle_error(args);
        return 1;
    }


    flag_handler_result flag_result =
        flag_handler(flags);

    if (flag_result.error)
    {
        flag_handler_handle_error(flag_result);
        return 1;
    }

    if (flag_result.options.help)
    {
        flag_handler_print_help(argv[0]);
        flag_handler_options_free(&flag_result.options);
        return 0;
    }


    if (files->count == 0)
    {
        fprintf(
            stderr,
            "no input files specified.\n"
            "use --help for more information.\n"
        );

        flag_handler_options_free(&flag_result.options);
        return 1;
    }

    if (
        files->c_source_count > 0 &&
        (
            flag_result.options.asm_output ||
            flag_result.options.ast ||
            flag_result.options.tokens ||
            flag_result.options.format
        )
    ) {
        fprintf(
            stderr,
            "C source files can only be used when building an executable.\n"
        );
        flag_handler_options_free(&flag_result.options);
        return 1;
    }

    if (flag_result.options.format)
    {
        int format_result = formatter_run(files);
        flag_handler_options_free(&flag_result.options);
        return format_result;
    }


    files_handler_result files_result =
        files_handler(files);

    if (files_result.error)
    {
        files_handler_handle_error(files_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }


    lexer_result lexer_result =
        lexer(files_result);

    if (lexer_result.error)
    {
        lexer_handle_error(lexer_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }

    if (flag_result.options.tokens)
    {
        lexer_print_tokens(lexer_result);
        flag_handler_options_free(&flag_result.options);
        return 0;
    }


    parser_result parser_result =
        parser(lexer_result);

    if (parser_result.error)
    {
        parser_handle_error(parser_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }

    if (flag_result.options.ast)
    {
        parser_print_ast(parser_result.tree);
        flag_handler_options_free(&flag_result.options);
        return 0;
    }


    semantic_result semantic_result =
        semantic(parser_result);

    if (semantic_result.error)
    {
        semantic_handle_error(semantic_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }

    ir_result ir_result =
        ir_lower(
            parser_result.tree,
            flag_result.options.optimize
        );

    if (ir_result.error)
    {
        fprintf(
            stderr,
            "IR error: %s\n",
            ir_result.error_text
                ? ir_result.error_text
                : "IR lowering failed"
        );
        ir_result_free(&ir_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }


    codegen_result codegen_result =
        codegen(
            ir_result.module,
            &flag_result.options
        );
    ir_result_free(&ir_result);

    if (codegen_result.error)
    {
        codegen_handle_error(codegen_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }


    output_result output_result =
        output(
            codegen_result,
            &flag_result.options,
            files
        );

    if (output_result.error)
    {
        output_handle_error(output_result);
        flag_handler_options_free(&flag_result.options);
        return 1;
    }

    flag_handler_options_free(&flag_result.options);
    return 0;
}
