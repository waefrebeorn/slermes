#ifndef HERMES_PLUGIN_INDEX_PURE_H
#define HERMES_PLUGIN_INDEX_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/plugin_index.py.
 * All char* returns are malloc'd (free()). */

char *pindex_install_identifier(const char *repo, const char *subdir);
char *pindex_parse_entries(const char *raw_json);
char *pindex_resolve_name(const char *entries_json, const char *name);
char *pindex_search_index(const char *entries_json, const char *term, const char *capability);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PLUGIN_INDEX_PURE_H */
