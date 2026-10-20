#include "public.h"
#include "internal.h"

#include <stdio.h>
#include <string.h>

static flag_handler_result
flag_handler_error(
    const char *message
)
{
    return (flag_handler_result) {
        .error = 1,
        .error_text = (char *)message
    };
}

flag_handler_result
flag_handler(
    flags *flags
)
{
    flag_handler_options options = {
        .target = FLAG_TARGET_X86_64,
        .help = 0,
        .ast = 0,
        .asm_output = 0,
        .format = 0,
        .tokens = 0,
        .version = 0,
        .optimize = 0,
        .executable = 1,
        .output_path = NULL,
    };

    if (!flags) {
        return flag_handler_error("no flags provided.");
    }

    for (int index = 0; index < flags->count; index++)
    {
        const char *flag = flags->values[index];

        if (flag_handler_is_help(flag)) {
            options.help = 1;
            continue;
        }

        if (flag_handler_is_ast(flag)) {
            options.ast = 1;
            continue;
        }

        if (flag_handler_is_asm(flag)) {
            options.asm_output = 1;
            options.executable = 0;
            continue;
        }

        if (flag_handler_is_format(flag)) {
            options.format = 1;
            options.executable = 0;
            continue;
        }

        if (flag_handler_is_tokens(flag)) {
            options.tokens = 1;
            continue;
        }

        if (flag_handler_is_optimize(flag)) {
            options.optimize = 1;
            continue;
        }

        if (strcmp(flag, "-o") == 0) {
            if (index + 1 >= flags->count) {
                return flag_handler_error(
                    "missing output path after -o"
                );
            }

            options.output_path = flags->values[++index];
            continue;
        }

        if (flag_handler_is_version(flag)) {
            options.version = 1;
            continue;
        }

        if (flag_handler_is_target(flag))
        {
            if (flag_handler_parse_target(
                    flag,
                    &options.target
                ))
            {
                return flag_handler_error(
                    "unknown target. available targets: x86_64"
                );
            }

            continue;
        }

        return flag_handler_error("unknown flag");
    }

    if (
        options.format &&
        (
            options.ast ||
            options.asm_output ||
            options.tokens ||
            options.optimize ||
            options.output_path != NULL
        )
    ) {
        return flag_handler_error(
            "--format cannot be combined with other output modes, -O1, or -o"
        );
    }

    return (flag_handler_result) {
        .error = 0,
        .error_text = NULL,
        .options = options
    };
}

void
flag_handler_handle_error(
    flag_handler_result result
)
{
    if (!result.error) {
        return;
    }

    fprintf(
        stderr,
        "flag error: %s\n",
        result.error_text
            ? result.error_text
            : "unknown error"
    );

    fprintf(
        stderr,
        "use 'lowc --help' for usage information\n"
    );
}

void
flag_handler_print_help(
    const char *program
)
{
    if (!program) {
        program = "lowc";
    }

    printf(
        "lowC compiler\n"
        "\n"
        "Usage:\n"
        "  %s [options] <source>\n"
        "\n"
        "Options:\n"
        "  --help              Show this help message and exit.\n"
        "\n"
        "Output:\n"
        "  --tokens            Print lexer tokens.\n"
        "  --ast               Print the parsed abstract syntax tree.\n"
        "  --asm               Print assembly output instead of building an executable.\n"
        "  --format            Format source files in place (requires clang-format).\n"
        "  -o <path>           Write the executable to <path> (default: a.out).\n"
        "  -O1                 Enable lightweight assembly optimizations.\n"
        "\n"
        "Target:\n"
        "  --target=x86_64     Compile for x86-64.\n"
        "\n"
        "Examples:\n"
        "  %s program.lowc\n"
        "  %s --tokens program.lowc\n"
        "  %s --ast program.lowc\n"
        "  %s --asm program.lowc\n"
        "  %s --format program.lowc\n"
        "  %s -O1 -o program program.lowc\n"
        "  %s --target=x86_64 program.lowc\n"
        "\n",
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program
    );
}
