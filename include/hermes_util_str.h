#ifndef HERMES_UTIL_STR_H
#define HERMES_UTIL_STR_H

/* Shared, single-definition string / path utilities.
 *
 * These previously existed as copy-pasted `static` helpers in several port
 * files with DIVERGENT behavior (e.g. some copies of hermes_home_dir omitted
 * the SLERMES_HOME env var). They are now defined ONCE here so every TU gets
 * identical, correct behavior. Include this header instead of re-defining.
 */

#include <stddef.h>

/* Resolve the shared Hermes home directory.
 * Precedence: HERMES_HOME, then SLERMES_HOME, then HOME/.hermes,
 * then "./.hermes" if HOME is unset. Writes into out (sz bytes). */
void hermes_home_dir(char *out, size_t sz);

#endif /* HERMES_UTIL_STR_H */
