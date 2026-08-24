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

/* Port of tools/tool_result_storage.py spillover helpers */
int tools_tool_result_storage_get_spillover_dir(char *out, size_t sz);
int tools_tool_result_storage_cleanup_spillover_cache(int max_age_hours);
int tools_tool_result_storage_extract_persisted_path(const char *content,
                                                     char *out, size_t sz);

#endif /* SLERMES_RESULT_STORAGE_H */
