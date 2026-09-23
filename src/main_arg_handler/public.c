#include "public.h"
#include "internal.h"


main_arg_handler_result
main_arg_handler(
    int argc,
    char **argv,
    flags **flags,
    files **files
)
{
    return main_arg_handler_internal(
        argc,
        argv,
        flags,
        files
    );
}


void
main_arg_handler_handle_error(
    main_arg_handler_result result
)
{
    main_arg_handler_handle_error_internal(result);
}
