#ifndef LOWC_SEMANTIC_ERROR_H
#define LOWC_SEMANTIC_ERROR_H

#include "public.h"

semantic_result
semantic_error(
    ast *node,
    const char *message
);

#endif
