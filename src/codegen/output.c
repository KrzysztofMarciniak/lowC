#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
codegen_output_grow(
    codegen_output *output,
    size_t required
)
{
    size_t capacity;
    char *data;

    if (required <= output->capacity)
        return 0;

    capacity = output->capacity;

    if (capacity == 0)
        capacity = 256;

    while (capacity < required)
        capacity *= 2;

    data = realloc(
        output->data,
        capacity
    );

    if (!data)
        return 1;

    output->data = data;
    output->capacity = capacity;

    return 0;
}

void
codegen_output_init(
    codegen_output *output
)
{
    output->data = NULL;
    output->size = 0;
    output->capacity = 0;
}

void
codegen_output_free(
    codegen_output *output
)
{
    free(output->data);

    output->data = NULL;
    output->size = 0;
    output->capacity = 0;
}

int
codegen_output_write(
    codegen_output *output,
    const char *text
)
{
    size_t length;

    if (!text)
        return 1;

    length = strlen(text);

    if (codegen_output_grow(
            output,
            output->size + length + 1
        ))
        return 1;

    memcpy(
        output->data + output->size,
        text,
        length
    );

    output->size += length;
    output->data[output->size] = '\0';

    return 0;
}

int
codegen_output_printf(
    codegen_output *output,
    const char *format,
    ...
)
{
    va_list arguments;
    va_list copy;
    int length;

    va_start(arguments, format);
    va_copy(copy, arguments);

    length = vsnprintf(
        NULL,
        0,
        format,
        copy
    );

    va_end(copy);

    if (length < 0)
    {
        va_end(arguments);
        return 1;
    }

    if (codegen_output_grow(
            output,
            output->size + (size_t)length + 1
        ))
    {
        va_end(arguments);
        return 1;
    }

    vsnprintf(
        output->data + output->size,
        (size_t)length + 1,
        format,
        arguments
    );

    va_end(arguments);

    output->size += (size_t)length;

    return 0;
}
