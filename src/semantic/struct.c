#include "struct.h"

#include <stdlib.h>
#include <string.h>

semantic_struct *
semantic_struct_create(
    ast *declaration
)
{
    semantic_struct *result;

    result = malloc(sizeof(semantic_struct));

    if (result == NULL)
        return NULL;

    result->name = malloc(
        strlen(declaration->structure.name) + 1
    );

    if (result->name == NULL)
    {
        free(result);
        return NULL;
    }

    strcpy(result->name, declaration->structure.name);

    result->declaration = declaration;

    return result;
}

semantic_struct *
semantic_struct_find(
    semantic_struct **structs,
    unsigned long count,
    const char *name
)
{
    unsigned long i;

    for (i = 0; i < count; i++)
    {
        if (strcmp(structs[i]->name, name) == 0)
            return structs[i];
    }

    return NULL;
}

ast *
semantic_struct_find_field(
    ast *structure,
    const char *name
)
{
    unsigned long i;

    if (structure == NULL)
        return NULL;

    for (i = 0; i < structure->structure.fields.count; i++)
    {
        ast *field;

        field = structure->structure.fields.values[i];

        if (strcmp(field->struct_field.name, name) == 0)
            return field;
    }

    return NULL;
}

void
semantic_struct_free(
    semantic_struct *structure
)
{
    if (structure == NULL)
        return;

    free(structure->name);
    free(structure);
}

semantic_type *
semantic_type_resolve(
    const char *name,
    unsigned long pointer_depth,
    semantic_struct **structs,
    unsigned long struct_count
)
{
    semantic_type *type;

    type = semantic_type_builtin(name);

    if (type == NULL)
    {
        if (semantic_struct_find(structs, struct_count, name) == NULL)
            return NULL;

        type = semantic_type_struct(name);

        if (type == NULL)
            return NULL;
    }

    while (pointer_depth > 0)
    {
        semantic_type *pointer;

        pointer = semantic_type_pointer(type);

        if (pointer == NULL)
        {
            semantic_type_free(type);
            return NULL;
        }

        type = pointer;
        pointer_depth--;
    }

    return type;
}
