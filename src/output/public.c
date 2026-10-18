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
    const char *output_path
)
{
    char temporary_path[] = "/tmp/lowc-XXXXXX";
    const char *compiler;
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

    child = fork();
    if (child < 0) {
        unlink(temporary_path);
        return output_error("failed to start assembler and linker");
    }

    if (child == 0) {
        compiler = getenv("CC");
        if (compiler == NULL || compiler[0] == '\0') {
            compiler = "cc";
        }

        execlp(
            compiler,
            compiler,
            "-x",
            "assembler",
            temporary_path,
            "-o",
            output_path,
            (char *)NULL
        );
        _exit(127);
    }

    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

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
    flag_handler_options *options
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
            : "a.out"
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
