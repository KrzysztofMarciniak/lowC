#ifndef LOWC_MAIN_ARG_HANDLER_INTERNAL_H
#define LOWC_MAIN_ARG_HANDLER_INTERNAL_H

#include "public.h"


main_arg_handler_result
main_arg_handler_internal(
    int argc,
    char **argv,
    flags **flags,
    files **files
);


#endif

void
main_arg_handler_handle_error_internal(
    main_arg_handler_result result
);
