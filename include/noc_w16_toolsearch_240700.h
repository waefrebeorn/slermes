#ifndef NOC_W16_TOOLSEARCH_240700_H
#define NOC_W16_TOOLSEARCH_240700_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tools/tool_search_validation.py (gate w16_toolsearch_240700).
 * Registry/session lookups are injected from the fixture; all string building
 * and schema logic mirrors the live Python exactly. */

json_t *w16_toolsearch_240700_schema_for_local_validation(const json_t *node);
json_t *w16_toolsearch_240700_schema_has_external_ref(const json_t *node);
json_t *w16_toolsearch_240700_normalize_tool_call_entries(const json_t *args,
                                                          const json_t *bridge_names);
json_t *w16_toolsearch_240700_local_batch_error(const json_t *entries);
json_t *w16_toolsearch_240700_unavailable_in_session_reason(const char *name);
json_t *w16_toolsearch_240700_session_unavailable_error(const char *name,
                                                        const json_t *session_names,
                                                        int core_and_registered,
                                                        const char *out_of_scope);
json_t *w16_toolsearch_240700_not_deferrable_error(const char *name,
                                                   const json_t *session_names,
                                                   int core_or_registered,
                                                   const json_t *all_tool_names);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_TOOLSEARCH_240700_H */
