#ifndef LOWC_SEMANTIC_STATEMENT_H
#define LOWC_SEMANTIC_STATEMENT_H

#include "expression.h"

/**
 * @brief Check one statement (and, recursively, everything it contains).
 *
 * @return 0 on success, 1 on error (with *error_text set to a newly
 *         allocated message).
 */
int
semantic_check_statement(
    semantic_context *context,
    ast *statement,
    char **error_text
);

/**
 * @brief Check whether a statement guarantees an explicit return.
 *
 * @return Nonzero if every normal path through the statement returns.
 */
int
semantic_statement_always_returns(
    const ast *statement
);

#endif
