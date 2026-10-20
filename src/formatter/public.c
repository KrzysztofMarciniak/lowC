#define _POSIX_C_SOURCE 200809L
#include "public.h"

#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char formatter_style[] =
    "--style={BasedOnStyle: Google, IndentWidth: 8, ColumnLimit: 80, "
    "SpaceBeforeParens: ControlStatements, SpacesInParentheses: false, "
    "AlignConsecutiveAssignments: true, SortIncludes: true, "
    "SpacesBeforeTrailingComments: 0, AlwaysBreakTemplateDeclarations: false, "
    "SpaceAfterCStyleCast: false, DerivePointerAlignment: false, "
    "PointerAlignment: Left, SeparateDefinitionBlocks: Always, UseTab: Never}";

static int
formatter_is_source(
    const char *path
)
{
    const char *extension = strrchr(path, '.');

    return extension != NULL && strcmp(extension, ".lowc") == 0;
}

static int
formatter_run_file(
    const char *path
)
{
    int status;
    pid_t child;
    pid_t waited;

    child = fork();
    if (child < 0) {
        fprintf(
            stderr,
            "format error: failed to start clang-format: %s\n",
            strerror(errno)
        );
        return 1;
    }

    if (child == 0) {
        execlp(
            "clang-format",
            "clang-format",
            formatter_style,
            "--assume-filename=source.c",
            "-i",
            "--",
            path,
            (char *)NULL
        );

        fprintf(
            stderr,
            "format error: failed to execute clang-format: %s\n",
            strerror(errno)
        );
        _exit(127);
    }

    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        fprintf(
            stderr,
            "format error: failed to wait for clang-format: %s\n",
            strerror(errno)
        );
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }

    if (WIFEXITED(status)) {
        fprintf(
            stderr,
            "format error: clang-format exited with status %d\n",
            WEXITSTATUS(status)
        );
    } else if (WIFSIGNALED(status)) {
        fprintf(
            stderr,
            "format error: clang-format was terminated by signal %d\n",
            WTERMSIG(status)
        );
    } else {
        fprintf(stderr, "format error: clang-format did not complete\n");
    }

    return 1;
}

int
formatter_run(
    files *inputs
)
{
    if (inputs == NULL || inputs->count == 0) {
        fprintf(stderr, "format error: no input files provided\n");
        return 1;
    }

    for (int index = 0; index < inputs->count; index++) {
        glob_t matches = {0};
        int result = glob(inputs->values[index], 0, NULL, &matches);

        if (result == GLOB_NOMATCH) {
            fprintf(
                stderr,
                "format error: no source file matches '%s'\n",
                inputs->values[index]
            );
            globfree(&matches);
            return 1;
        }

        if (result != 0) {
            fprintf(
                stderr,
                "format error: failed to expand source path '%s'\n",
                inputs->values[index]
            );
            globfree(&matches);
            return 1;
        }

        for (size_t match = 0; match < matches.gl_pathc; match++) {
            const char *path = matches.gl_pathv[match];

            if (!formatter_is_source(path)) {
                fprintf(
                    stderr,
                    "format error: input is not a .lowc source file: %s\n",
                    path
                );
                globfree(&matches);
                return 1;
            }

            if (formatter_run_file(path) != 0) {
                globfree(&matches);
                return 1;
            }
        }

        globfree(&matches);
    }

    return 0;
}
