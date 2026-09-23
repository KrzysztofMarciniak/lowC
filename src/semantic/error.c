#include "error.h"

#include <stdlib.h>
#include <string.h>

semantic_result
semantic_error(
    ast *node,
    const char *message
)
{
    semantic_result result;

    memset(&result, 0, sizeof(result));

    result.error = 1;

    result.error_text = malloc(strlen(message) + 1);

    if (result.error_text != NULL)
        strcpy(result.error_text, message);

    if (node != NULL)
    {
        result.file = node->file;
        result.line = node->line;
        result.column = node->column;
    }

    return result;
}
