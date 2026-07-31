/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_RESULT_STORAGE_H
#define SLERMES_RESULT_STORAGE_H

#include "hermes_core_types.h"

/* Tool result storage + preview */
char *tool_result_store(const char *data, size_t size, size_t max_inline);
void  tool_result_cleanup(int max_age_seconds);
char *generate_preview(const char *content, int max_chars, bool *has_more);

#endif /* SLERMES_RESULT_STORAGE_H */
