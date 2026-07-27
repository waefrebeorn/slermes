/* truthy.h — shared truthy coercion, faithful C11 port of utils.py.
 * The project's shared truthy string set: {"1","true","yes","on"}.
 * Implemented in src/cli/port_utils_truthy.c. Self-contained.
 */

#ifndef SLERMES_TRUTHY_H
#define SLERMES_TRUTHY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Coerce a bool-ish string using the shared truthy set. NULL -> default.
 * Strips whitespace, lowercases, membership-tests {"1","true","yes","on"}.
 * Non-empty non-truthy strings -> false (mirrors Python str branch). */
bool is_truthy_value(const char *value, bool default_value);

/* True when environment variable `name` is set to a truthy value. */
bool env_var_enabled(const char *name, const char *default_value);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_TRUTHY_H */
