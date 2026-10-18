#ifndef LOWC_FLAG_HANDLER_INTERNAL_H
#define LOWC_FLAG_HANDLER_INTERNAL_H

#include "public.h"

int
flag_handler_is_help(
    const char *flag
);

int
flag_handler_is_ast(
    const char *flag
);

int
flag_handler_is_asm(
    const char *flag
);

int
flag_handler_is_tokens(
    const char *flag
);

int
flag_handler_is_version(
    const char *flag
);

int
flag_handler_is_optimize(
    const char *flag
);

int
flag_handler_is_target(
    const char *flag
);

int
flag_handler_parse_target(
    const char *flag,
    flag_target *target
);

#endif
