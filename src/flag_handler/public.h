#ifndef LOWC_FLAG_HANDLER_PUBLIC_H
#define LOWC_FLAG_HANDLER_PUBLIC_H

#include "main_arg_handler/public.h"

typedef struct foreign_driver_mapping
{
    char *language;
    char *driver;
} foreign_driver_mapping;

typedef enum flag_target
{
    FLAG_TARGET_X86_64
} flag_target;

typedef struct flag_handler_options
{
    flag_target target;

    int help;
    int ast;
    int asm_output;
    int format;
    int tokens;
    int version;
    int debug;
    int optimize;
    int executable;
    const char *output_path;
    char **linker_arguments;
    int linker_argument_count;
    foreign_driver_mapping *foreign_drivers;
    int foreign_driver_count;
    char *c_driver;
} flag_handler_options;

typedef struct flag_handler_result
{
    int error;
    char *error_text;
    flag_handler_options options;
} flag_handler_result;

flag_handler_result
flag_handler(
    flags *flags
);

void
flag_handler_handle_error(
    flag_handler_result result
);

void
flag_handler_options_free(
    flag_handler_options *options
);

void 
flag_handler_print_help( const char *program );


#endif
