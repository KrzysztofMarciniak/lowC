#define _POSIX_C_SOURCE 200809L
#include "public.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char *
output_strdup(
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

static output_result
output_error(
    const char *message
)
{
    return (output_result) {
        .error = 1,
        .error_text = output_strdup(message)
    };
}

static output_result
output_build_executable(
    const char *assembly,
    const char *output_path,
    const flag_handler_options *options,
    const files *input_files
)
{
    char temporary_path[] = "/tmp/lowc-XXXXXX";
    const char *compiler;
    char **compiler_arguments;
    size_t argument_index;
    size_t argument_count;
    size_t c_source_argument_count;
    size_t optimization_argument_count;
    size_t strip_argument_count;
    size_t remaining;
    const char *cursor;
    int descriptor;
    int status;
    pid_t child;
    pid_t waited;

    descriptor = mkstemp(temporary_path);
    if (descriptor < 0) {
        return output_error("failed to create temporary assembly file");
    }

    cursor = assembly;
    remaining = strlen(assembly);

    while (remaining > 0) {
        ssize_t written = write(descriptor, cursor, remaining);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            close(descriptor);
            unlink(temporary_path);
            return output_error("failed to write temporary assembly file");
        }

        cursor += written;
        remaining -= (size_t)written;
    }

    if (close(descriptor) != 0) {
        unlink(temporary_path);
        return output_error("failed to close temporary assembly file");
    }

    compiler = options->c_driver;
    if (compiler == NULL || compiler[0] == '\0') {
        compiler = getenv("CC");
    }
    if (compiler == NULL || compiler[0] == '\0') {
        compiler = "cc";
    }

    c_source_argument_count =
        input_files->c_source_count == 0
            ? 0
            : 2 + (size_t)input_files->c_source_count;
    optimization_argument_count = options->optimize ? 5 : 0;
    strip_argument_count =
        options->optimize && !options->debug ? 1 : 0;
    argument_count =
        7 +
        (options->debug ? 1 : 0) +
        optimization_argument_count +
        strip_argument_count +
        (size_t)options->linker_argument_count +
        c_source_argument_count;
    compiler_arguments = calloc(
        argument_count,
        sizeof(*compiler_arguments)
    );
    if (compiler_arguments == NULL) {
        unlink(temporary_path);
        return output_error("failed to allocate assembler and linker arguments");
    }

    compiler_arguments[0] = (char *)compiler;
    compiler_arguments[1] = "-x";
    compiler_arguments[2] = "assembler";
    compiler_arguments[3] = temporary_path;
    compiler_arguments[4] = "-o";
    compiler_arguments[5] = (char *)output_path;
    argument_index = 6;
    if (options->optimize) {
        compiler_arguments[argument_index++] = "-Os";
        compiler_arguments[argument_index++] = "-flto";
        compiler_arguments[argument_index++] = "-ffunction-sections";
        compiler_arguments[argument_index++] = "-fdata-sections";
        compiler_arguments[argument_index++] = "-Wl,--gc-sections";
    }
    if (options->debug) {
        compiler_arguments[argument_index++] = "-g";
    }
    if (options->optimize && !options->debug) {
        compiler_arguments[argument_index++] = "-s";
    }

    if (input_files->c_source_count > 0) {
        compiler_arguments[argument_index++] = "-x";
        compiler_arguments[argument_index++] = "c";
        for (
            int index = 0;
            index < input_files->c_source_count;
            index++
        ) {
            compiler_arguments[argument_index++] =
                input_files->c_sources[index];
        }
    }
    for (
        int index = 0;
        index < options->linker_argument_count;
        index++
    ) {
        compiler_arguments[argument_index++] =
            options->linker_arguments[index];
    }
    compiler_arguments[argument_index] = NULL;

    child = fork();
    if (child < 0) {
        free(compiler_arguments);
        unlink(temporary_path);
        return output_error("failed to start assembler and linker");
    }

    if (child == 0) {
        execvp(compiler, compiler_arguments);
        _exit(127);
    }

    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    free(compiler_arguments);
    unlink(temporary_path);

    if (waited < 0) {
        return output_error("failed to wait for assembler and linker");
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return output_error("assembler or linker failed");
    }

    return (output_result) {0};
}

output_result
output(
    codegen_result codegen,
    flag_handler_options *options,
    files *input_files
)
{
    output_result result = {
        0
    };

    if (codegen.error)
    {
        result.error = 1;
        result.error_text =
            output_strdup("cannot output invalid codegen result");

        return result;
    }

    if (options == NULL)
    {
        result.error = 1;
        result.error_text =
            output_strdup("missing compiler options");

        return result;
    }

    if (input_files == NULL)
    {
        result.error = 1;
        result.error_text =
            output_strdup("missing input files");

        return result;
    }

    if (codegen.assembly == NULL)
    {
        result.error = 1;
        result.error_text =
            output_strdup("no assembly was generated");

        return result;
    }

    if (options->asm_output) {
        if (fputs(codegen.assembly, stdout) == EOF) {
            return output_error("failed to write assembly");
        }

        return result;
    }

    return output_build_executable(
        codegen.assembly,
        options->output_path != NULL
            ? options->output_path
            : "a.out",
        options,
        input_files
    );
}

void
output_handle_error(
    output_result result
)
{
    if (!result.error)
        return;

    fprintf(
        stderr,
        "output error: %s\n",
        result.error_text
            ? result.error_text
            : "unknown output error"
    );
}

void
output_free(
    output_result result
)
{
    free(result.error_text);
}
