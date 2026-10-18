#include "internal.h"

#include <string.h>

int
flag_handler_is_help(
    const char *flag
)
{
    return strcmp(flag, "--help") == 0;
}

int
flag_handler_is_ast(
    const char *flag
)
{
    return strcmp(flag, "--ast") == 0;
}

int
flag_handler_is_asm(
    const char *flag
)
{
    return strcmp(flag, "--asm") == 0;
}

int
flag_handler_is_tokens(
    const char *flag
)
{
    return strcmp(flag, "--tokens") == 0;
}

int
flag_handler_is_version(
    const char *flag
)
{
    return strcmp(flag, "--version") == 0;
}

int
flag_handler_is_optimize(
    const char *flag
)
{
    return strcmp(flag, "-O1") == 0;
}

int
flag_handler_is_target(
    const char *flag
)
{
    return strncmp(flag, "--target=", 9) == 0;
}

int
flag_handler_parse_target(
    const char *flag,
    flag_target *target
)
{
    if (strcmp(flag, "--target=x86_64") == 0)
    {
        *target = FLAG_TARGET_X86_64;
        return 0;
    }

    return 1;
}
