#include "function.h"

#include <stdlib.h>
#include <string.h>

semantic_function *
semantic_function_create(
    ast *function,
    semantic_struct **structs,
    unsigned long struct_count
)
{
    semantic_function *result;
    unsigned long i;

    result = calloc(1, sizeof(semantic_function));

    if (result == NULL)
        return NULL;

    result->name = malloc(
        strlen(function->function.name) + 1
    );

    if (result->name == NULL)
    {
        free(result);
        return NULL;
    }

    strcpy(
        result->name,
        function->function.name
    );

    result->declaration = function;

    result->return_type = semantic_type_resolve(
        function->function.return_type,
        function->function.return_pointer_depth,
        structs,
        struct_count
    );

    if (result->return_type == NULL)
    {
        free(result->name);
        free(result);
        return NULL;
    }

    result->parameter_count =
        function->function.parameters.count;

    if (result->parameter_count != 0)
    {
        result->parameter_types = calloc(
            result->parameter_count,
            sizeof(semantic_type *)
        );

        if (result->parameter_types == NULL)
        {
            semantic_function_free(result);
            return NULL;
        }

        for (i = 0; i < result->parameter_count; i++)
        {
            ast *parameter;

            parameter =
                function->function.parameters.values[i];

            result->parameter_types[i] = semantic_type_resolve(
                parameter->parameter.type,
                parameter->parameter.pointer_depth,
                structs,
                struct_count
            );

            if (result->parameter_types[i] == NULL)
            {
                semantic_function_free(result);
                return NULL;
            }
        }
    }

    return result;
}

semantic_function *
semantic_function_find(
    semantic_function **functions,
    unsigned long count,
    const char *name
)
{
    unsigned long i;

    for (i = 0; i < count; i++)
    {
        if (strcmp(functions[i]->name, name) == 0)
            return functions[i];
    }

    return NULL;
}

void
semantic_function_free(
    semantic_function *function
)
{
    unsigned long i;

    if (function == NULL)
        return;

    free(function->name);

    semantic_type_free(function->return_type);

    if (function->parameter_types != NULL)
    {
        for (i = 0; i < function->parameter_count; i++)
            semantic_type_free(function->parameter_types[i]);
    }

    free(function->parameter_types);
    free(function);
}
