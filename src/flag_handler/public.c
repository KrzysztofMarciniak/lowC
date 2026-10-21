#include "public.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
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
        .debug = 0,
        .optimize = 1,
        .executable = 1,
        .output_path = NULL,
        .linker_arguments = NULL,
        .linker_argument_count = 0,
        .foreign_drivers = NULL,
        .foreign_driver_count = 0,
        .c_driver = NULL,
    };

    if (!flags) {
        return flag_handler_error("no flags provided.");
    }

    if (flags->count > 0) {
        options.linker_arguments = calloc(
            (size_t)flags->count,
            sizeof(*options.linker_arguments)
        );
        options.foreign_drivers = calloc(
            (size_t)flags->count,
            sizeof(*options.foreign_drivers)
        );
        if (!options.linker_arguments || !options.foreign_drivers) {
            flag_handler_options_free(&options);
            return flag_handler_error("out of memory.");
        }
    }

    int optimize_explicit = 0;

    for (int index = 0; index < flags->count; index++)
    {
        char *flag = flags->values[index];

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
            optimize_explicit = 1;
            continue;
        }

        if (flag_handler_is_debug(flag)) {
            options.debug = 1;
            continue;
        }

        if (strcmp(flag, "-o") == 0) {
            if (index + 1 >= flags->count) {
                flag_handler_options_free(&options);
                return flag_handler_error(
                    "missing output path after -o"
                );
            }

            options.output_path = flags->values[++index];
            continue;
        }

        if (strcmp(flag, "-foreign") == 0) {
            char *mapping;
            char *separator;
            size_t language_length;
            foreign_driver_mapping *driver_mapping;

            if (index + 1 >= flags->count) {
                flag_handler_options_free(&options);
                return flag_handler_error(
                    "missing mapping after -foreign"
                );
            }

            mapping = flags->values[++index];
            separator = strchr(mapping, '=');
            if (
                separator == NULL ||
                separator == mapping ||
                separator[1] == '\0'
            ) {
                flag_handler_options_free(&options);
                return flag_handler_error(
                    "expected -foreign <language>=<driver>"
                );
            }

            language_length = (size_t)(separator - mapping);
            for (
                int mapping_index = 0;
                mapping_index < options.foreign_driver_count;
                mapping_index++
            ) {
                driver_mapping =
                    &options.foreign_drivers[mapping_index];
                if (
                    strlen(driver_mapping->language) ==
                        language_length &&
                    strncmp(
                        driver_mapping->language,
                        mapping,
                        language_length
                    ) == 0
                ) {
                    flag_handler_options_free(&options);
                    return flag_handler_error(
                        "duplicate foreign language driver mapping"
                    );
                }
            }

            driver_mapping =
                &options.foreign_drivers[
                    options.foreign_driver_count++
                ];
            *separator = '\0';
            driver_mapping->language = mapping;
            driver_mapping->driver = separator + 1;

            if (
                language_length == 1 &&
                mapping[0] == 'c'
            ) {
                options.c_driver = driver_mapping->driver;
            }

            continue;
        }

        if (flag[0] == '-' && flag[1] == 'L') {
            if (flag[2] == '\0') {
                flag_handler_options_free(&options);
                return flag_handler_error(
                    "expected a path after -L"
                );
            }
            options.linker_arguments[
                options.linker_argument_count++
            ] = flag;
            continue;
        }

        if (flag[0] == '-' && flag[1] == 'l') {
            if (flag[2] == '\0') {
                flag_handler_options_free(&options);
                return flag_handler_error(
                    "expected a library name after -l"
                );
            }
            options.linker_arguments[
                options.linker_argument_count++
            ] = flag;
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
                flag_handler_options_free(&options);
                return flag_handler_error(
                    "unknown target. available targets: x86_64"
                );
            }

            continue;
        }

        flag_handler_options_free(&options);
        return flag_handler_error("unknown flag");
    }

    if (
        options.format &&
        (
            options.ast ||
            options.asm_output ||
            options.tokens ||
            optimize_explicit ||
            options.debug ||
            options.output_path != NULL ||
            options.linker_argument_count != 0 ||
            options.foreign_driver_count != 0
        )
    ) {
        flag_handler_options_free(&options);
        return flag_handler_error(
            "--format cannot be combined with other output modes, optimization/debug flags, -o, or linker options"
        );
    }

    if (
        options.debug &&
        (options.ast || options.tokens || options.format)
    ) {
        flag_handler_options_free(&options);
        return flag_handler_error(
            "-debug cannot be combined with --ast, --tokens, or --format"
        );
    }

    if (options.debug && !optimize_explicit) {
        options.optimize = 0;
    }

    return (flag_handler_result) {
        .error = 0,
        .error_text = NULL,
        .options = options
    };
}

void
flag_handler_options_free(
    flag_handler_options *options
)
{
    if (options == NULL)
        return;

    free(options->linker_arguments);
    free(options->foreign_drivers);
    options->linker_arguments = NULL;
    options->linker_argument_count = 0;
    options->foreign_drivers = NULL;
    options->foreign_driver_count = 0;
    options->c_driver = NULL;
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
        "  %s [options] <source.lowc> [source.c ...]\n"
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
        "  -optimize           Optimize IR and use -Os/LTO release linking by default.\n"
        "  -release            Alias for -optimize; strip release symbols.\n"
        "  -debug              Emit debug information and retain symbols; disable optimization unless requested.\n"
        "  -L<path>            Add a library search path.\n"
        "  -l<name>            Link a library.\n"
        "  -foreign <lang>=<driver> Select the driver for a foreign language (e.g. c=clang).\n"
        "  <source.c>          Compile C source files alongside LowC sources.\n"
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
        "  %s -optimize -o program program.lowc\n"
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
