#include <stdio.h>
#include <string.h>
#include "internal.h"


static int
main_arg_handler_is_flag(
    const char *argument
)
{
    if (argument == 0)
        return 0;

    if (argument[0] != '-')
        return 0;

    if (argument[1] == '\0')
        return 0;

    return 1;
}


static void
main_arg_handler_rotate(
    char **argv,
    int from,
    int to
)
{
    char *temporary;
    int position;


    temporary = argv[to];

    for (position = to; position > from; position--)
        argv[position] = argv[position - 1];

    argv[from] = temporary;
}


static void
main_arg_handler_split(
    int argc,
    char **argv,
    int *flag_count,
    int *file_count
)
{
    int position;
    int flag_position;
    int end_of_flags;


    *flag_count = 0;
    *file_count = 0;

    flag_position = 1;
    end_of_flags = 0;


    for (position = 1; position < argc; position++)
    {
        char *argument = argv[position];


        if (!end_of_flags &&
            argument[0] == '-' &&
            argument[1] == '-' &&
            argument[2] == '\0')
        {
            end_of_flags = 1;
            continue;
        }

        if (!end_of_flags &&
            strcmp(argument, "-o") == 0)
        {
            main_arg_handler_rotate(
                argv,
                flag_position,
                position
            );

            flag_position++;
            (*flag_count)++;

            main_arg_handler_rotate(
                argv,
                flag_position,
                position + 1
            );

            flag_position++;
            (*flag_count)++;
            position++;

            continue;
        }

        if (!end_of_flags &&
            main_arg_handler_is_flag(argument))
        {
            main_arg_handler_rotate(
                argv,
                flag_position,
                position
            );

            flag_position++;
            (*flag_count)++;

            continue;
        }


        (*file_count)++;
    }
}


main_arg_handler_result
main_arg_handler_internal(
    int argc,
    char **argv,
    flags **flags,
    files **files
)
{
    static struct flags flags_result;
    static struct files files_result;

    int flag_count;
    int file_count;
    int end_of_flags;

    if (argc < 1 || argv == 0)
    {
        return (main_arg_handler_result) {
            .error = 1,
            .error_text = "invalid argument list."
        };
    }

    if (flags == 0 || files == 0)
    {
        return (main_arg_handler_result) {
            .error = 1,
            .error_text = "invalid output pointers."
        };
    }

    end_of_flags = 0;
    for (int index = 1; index < argc; index++)
    {
        if (!end_of_flags && strcmp(argv[index], "--") == 0) {
            end_of_flags = 1;
            continue;
        }

        if (
            !end_of_flags &&
            strcmp(argv[index], "-o") == 0 &&
            index + 1 >= argc
        )
        {
            return (main_arg_handler_result) {
                .error = 1,
                .error_text = "missing output path after -o."
            };
        }

        if (!end_of_flags && strcmp(argv[index], "-o") == 0) {
            index++;
        }
    }


    flags_result.count = 0;
    flags_result.values = 0;

    files_result.count = 0;
    files_result.values = 0;


    main_arg_handler_split(
        argc,
        argv,
        &flag_count,
        &file_count
    );


    flags_result.count = flag_count;

    if (flag_count > 0)
        flags_result.values = &argv[1];


    files_result.count = file_count;

    if (file_count > 0)
        files_result.values = &argv[1 + flag_count];


    *flags = &flags_result;
    *files = &files_result;


    return (main_arg_handler_result) {
        .error = 0,
        .error_text = 0
    };
}


void
main_arg_handler_handle_error_internal(
    main_arg_handler_result result
)
{
    fprintf(
        stderr,
        "%s\n"
        "use --help for more information.\n",
        result.error_text
    );
}
