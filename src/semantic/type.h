#ifndef LOWC_SEMANTIC_TYPE_H
#define LOWC_SEMANTIC_TYPE_H

typedef enum semantic_type_kind
{
    SEMANTIC_TYPE_VOID,

    SEMANTIC_TYPE_U1,
    SEMANTIC_TYPE_U2,
    SEMANTIC_TYPE_U4,
    SEMANTIC_TYPE_U8,
    SEMANTIC_TYPE_U16,
    SEMANTIC_TYPE_U32,
    SEMANTIC_TYPE_U64,

    SEMANTIC_TYPE_S1,
    SEMANTIC_TYPE_S2,
    SEMANTIC_TYPE_S4,
    SEMANTIC_TYPE_S8,
    SEMANTIC_TYPE_S16,
    SEMANTIC_TYPE_S32,
    SEMANTIC_TYPE_S64,

    SEMANTIC_TYPE_POINTER,
    SEMANTIC_TYPE_ARRAY,
    SEMANTIC_TYPE_STRUCT
} semantic_type_kind;

typedef struct semantic_type semantic_type;

struct semantic_type
{
    semantic_type_kind kind;

    union
    {
        struct
        {
            semantic_type *element;
        } pointer;

        struct
        {
            semantic_type *element;
            unsigned long count;
        } array;

        struct
        {
            char *name;
        } structure;
    };
};

semantic_type *
semantic_type_builtin(
    const char *name
);

semantic_type *
semantic_type_pointer(
    semantic_type *element
);

semantic_type *
semantic_type_array(
    semantic_type *element,
    unsigned long count
);

semantic_type *
semantic_type_struct(
    const char *name
);

semantic_type *
semantic_type_copy(
    const semantic_type *type
);

int
semantic_type_equal(
    const semantic_type *left,
    const semantic_type *right
);

int
semantic_type_assignable(
    const semantic_type *destination,
    const semantic_type *source
);

unsigned long
semantic_type_size(
    const semantic_type *type
);

void
semantic_type_free(
    semantic_type *type
);

#endif
