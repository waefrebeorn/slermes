#ifndef NOC_W10_MEMORYTOOL_0052C2_H
#define NOC_W10_MEMORYTOOL_0052C2_H
#include "hermes_json.h"

/* PoP port of tools/memory_tool_store.py (gate w10_memorytool_0052c2). */
char   *w10_memorytool_0052c2_normalize_for_match(const char *text);
int     w10_memorytool_0052c2_span_occurrences(const char *text, const char *span);
char   *w10_memorytool_0052c2_repeated_span_message(const char *old_text, int count);
char   *w10_memorytool_0052c2_over_limit(const char *new_total, const char *limit);
char   *w10_memorytool_0052c2_stale_entry_message(const char *entry);
json_t *w10_memorytool_0052c2_find_unique_match(json_t *entries, const char *old_text);
#endif /* NOC_W10_MEMORYTOOL_0052C2_H */
