#ifndef LOWC_MAIN_ARG_HANDLER_PUBLIC_H
#define LOWC_MAIN_ARG_HANDLER_PUBLIC_H


typedef struct flags
{
    int count;
    char **values;
} flags;


typedef struct files
{
    int count;
    char **values;
} files;


typedef struct main_arg_handler_result
{
    int error;
    const char *error_text;
} main_arg_handler_result;


main_arg_handler_result
main_arg_handler(
    int argc,
    char **argv,
    flags **flags,
    files **files
);


void
main_arg_handler_handle_error(
    main_arg_handler_result result
);


#endif
