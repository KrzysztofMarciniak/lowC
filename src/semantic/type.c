#include "type.h"

#include <stdlib.h>
#include <string.h>


static char *
semantic_type_strdup(
    const char *value
)
{
    size_t length;
    char *copy;


    if (value == NULL)
        return NULL;


    length = strlen(value);

    copy = malloc(length + 1);

    if (copy == NULL)
        return NULL;


    memcpy(copy, value, length + 1);

    return copy;
}


/*
 * Return non-zero when the type is one of lowC's builtin
 * integer types.
 */
static int
semantic_type_is_integer(
    const semantic_type *type
)
{
    if (type == NULL)
        return 0;


    switch (type->kind)
    {
        case SEMANTIC_TYPE_U1:
        case SEMANTIC_TYPE_U2:
        case SEMANTIC_TYPE_U4:
        case SEMANTIC_TYPE_U8:
        case SEMANTIC_TYPE_U16:
        case SEMANTIC_TYPE_U32:
        case SEMANTIC_TYPE_U64:

        case SEMANTIC_TYPE_S1:
        case SEMANTIC_TYPE_S2:
        case SEMANTIC_TYPE_S4:
        case SEMANTIC_TYPE_S8:
        case SEMANTIC_TYPE_S16:
        case SEMANTIC_TYPE_S32:
        case SEMANTIC_TYPE_S64:

            return 1;

        default:
            return 0;
    }
}


semantic_type *
semantic_type_builtin(
    const char *name
)
{
    semantic_type *type;


    type = malloc(sizeof(semantic_type));

    if (type == NULL)
        return NULL;


    if (strcmp(name, "void") == 0)
        type->kind = SEMANTIC_TYPE_VOID;

    else if (strcmp(name, "u1") == 0)
        type->kind = SEMANTIC_TYPE_U1;

    else if (strcmp(name, "u2") == 0)
        type->kind = SEMANTIC_TYPE_U2;

    else if (strcmp(name, "u4") == 0)
        type->kind = SEMANTIC_TYPE_U4;

    else if (strcmp(name, "u8") == 0)
        type->kind = SEMANTIC_TYPE_U8;

    else if (strcmp(name, "u16") == 0)
        type->kind = SEMANTIC_TYPE_U16;

    else if (strcmp(name, "u32") == 0)
        type->kind = SEMANTIC_TYPE_U32;

    else if (strcmp(name, "u64") == 0)
        type->kind = SEMANTIC_TYPE_U64;

    else if (strcmp(name, "s1") == 0)
        type->kind = SEMANTIC_TYPE_S1;

    else if (strcmp(name, "s2") == 0)
        type->kind = SEMANTIC_TYPE_S2;

    else if (strcmp(name, "s4") == 0)
        type->kind = SEMANTIC_TYPE_S4;

    else if (strcmp(name, "s8") == 0)
        type->kind = SEMANTIC_TYPE_S8;

    else if (strcmp(name, "s16") == 0)
        type->kind = SEMANTIC_TYPE_S16;

    else if (strcmp(name, "s32") == 0)
        type->kind = SEMANTIC_TYPE_S32;

    else if (strcmp(name, "s64") == 0)
        type->kind = SEMANTIC_TYPE_S64;

    else
    {
        free(type);

        return NULL;
    }


    return type;
}


semantic_type *
semantic_type_pointer(
    semantic_type *element
)
{
    semantic_type *type;


    type = malloc(sizeof(semantic_type));

    if (type == NULL)
        return NULL;


    type->kind = SEMANTIC_TYPE_POINTER;
    type->pointer.element = element;

    return type;
}


semantic_type *
semantic_type_array(
    semantic_type *element,
    unsigned long count
)
{
    semantic_type *type;


    type = malloc(sizeof(semantic_type));

    if (type == NULL)
        return NULL;


    type->kind = SEMANTIC_TYPE_ARRAY;
    type->array.element = element;
    type->array.count = count;

    return type;
}


semantic_type *
semantic_type_struct(
    const char *name
)
{
    semantic_type *type;


    type = malloc(sizeof(semantic_type));

    if (type == NULL)
        return NULL;


    type->kind = SEMANTIC_TYPE_STRUCT;

    type->structure.name =
        semantic_type_strdup(name);


    if (type->structure.name == NULL)
    {
        free(type);

        return NULL;
    }


    return type;
}


semantic_type *
semantic_type_copy(
    const semantic_type *type
)
{
    semantic_type *copy;


    if (type == NULL)
        return NULL;


    switch (type->kind)
    {
        case SEMANTIC_TYPE_POINTER:

            copy = semantic_type_pointer(
                semantic_type_copy(
                    type->pointer.element
                )
            );

            return copy;


        case SEMANTIC_TYPE_ARRAY:

            copy = semantic_type_array(
                semantic_type_copy(
                    type->array.element
                ),
                type->array.count
            );

            return copy;


        case SEMANTIC_TYPE_STRUCT:

            return semantic_type_struct(
                type->structure.name
            );


        default:

            copy = malloc(
                sizeof(semantic_type)
            );

            if (copy == NULL)
                return NULL;


            copy->kind = type->kind;

            return copy;
    }
}


int
semantic_type_equal(
    const semantic_type *left,
    const semantic_type *right
)
{
    if (left == NULL || right == NULL)
        return 0;


    if (left->kind != right->kind)
        return 0;


    switch (left->kind)
    {
        case SEMANTIC_TYPE_POINTER:

            return semantic_type_equal(
                left->pointer.element,
                right->pointer.element
            );


        case SEMANTIC_TYPE_ARRAY:

            return
                left->array.count ==
                    right->array.count &&
                semantic_type_equal(
                    left->array.element,
                    right->array.element
                );


        case SEMANTIC_TYPE_STRUCT:

            return strcmp(
                left->structure.name,
                right->structure.name
            ) == 0;


        default:

            return 1;
    }
}


/*
 * Check whether one value can be assigned to another.
 *
 * All builtin integer types are assignment-compatible.
 *
 * This is intentional because integer literals currently have
 * the semantic type s64. Therefore:
 *
 *     u1 a = 1;
 *
 * sees:
 *
 *     destination: u1
 *     source:      s64
 *
 * and must be accepted.
 *
 * Other types still require exact type equality.
 */
int
semantic_type_assignable(
    const semantic_type *destination,
    const semantic_type *source
)
{
    if (destination == NULL || source == NULL)
        return 0;


    if (
        semantic_type_is_integer(destination) &&
        semantic_type_is_integer(source)
    )
        return 1;

    if (
        destination->kind == SEMANTIC_TYPE_POINTER &&
        source->kind == SEMANTIC_TYPE_POINTER &&
        (
            destination->pointer.element->kind == SEMANTIC_TYPE_VOID ||
            source->pointer.element->kind == SEMANTIC_TYPE_VOID
        )
    )
        return 1;

    return semantic_type_equal(
        destination,
        source
    );
}


unsigned long
semantic_type_size(
    const semantic_type *type
)
{
    if (type == NULL)
        return 0;


    switch (type->kind)
    {
        case SEMANTIC_TYPE_VOID:
            return 0;


        case SEMANTIC_TYPE_U1:
        case SEMANTIC_TYPE_U2:
        case SEMANTIC_TYPE_U4:
        case SEMANTIC_TYPE_U8:
            return 1;


        case SEMANTIC_TYPE_U16:
        case SEMANTIC_TYPE_S16:
            return 2;


        case SEMANTIC_TYPE_U32:
        case SEMANTIC_TYPE_S32:
            return 4;


        case SEMANTIC_TYPE_U64:
        case SEMANTIC_TYPE_S64:
            return 8;


        case SEMANTIC_TYPE_S1:
        case SEMANTIC_TYPE_S2:
        case SEMANTIC_TYPE_S4:
        case SEMANTIC_TYPE_S8:
            return 1;


        case SEMANTIC_TYPE_POINTER:
            return 8;


        case SEMANTIC_TYPE_ARRAY:

            return
                semantic_type_size(
                    type->array.element
                ) *
                type->array.count;


        case SEMANTIC_TYPE_STRUCT:
            return 0;
    }


    return 0;
}


void
semantic_type_free(
    semantic_type *type
)
{
    if (type == NULL)
        return;


    switch (type->kind)
    {
        case SEMANTIC_TYPE_POINTER:

            semantic_type_free(
                type->pointer.element
            );

            break;


        case SEMANTIC_TYPE_ARRAY:

            semantic_type_free(
                type->array.element
            );

            break;


        case SEMANTIC_TYPE_STRUCT:

            free(type->structure.name);

            break;


        default:
            break;
    }


    free(type);
}
