#ifndef LOWC_SEMANTIC_STRUCT_H
#define LOWC_SEMANTIC_STRUCT_H

#include "type.h"
#include "../parser/public.h"

typedef struct semantic_struct
{
    char *name;
    ast *declaration;
} semantic_struct;

semantic_struct *
semantic_struct_create(
    ast *declaration
);

semantic_struct *
semantic_struct_find(
    semantic_struct **structs,
    unsigned long count,
    const char *name
);

ast *
semantic_struct_find_field(
    ast *structure,
    const char *name
);

void
semantic_struct_free(
    semantic_struct *structure
);

/**
 * @brief Resolve a type name (with a pointer depth) to a semantic_type.
 *
 * Tries the builtin types first (u1/u2/u4/u8, s1/s2/s4/s8, void), then falls
 * back to the registered struct list. This is the single place type
 * names get turned into semantic_type objects, so builtins and struct
 * types are resolved consistently everywhere (globals, parameters,
 * return types, local variables).
 *
 * @return A newly allocated type, or NULL if the name is unknown or
 *         allocation failed.
 */
semantic_type *
semantic_type_resolve(
    const char *name,
    unsigned long pointer_depth,
    semantic_struct **structs,
    unsigned long struct_count
);

#endif
