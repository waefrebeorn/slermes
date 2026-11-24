#ifndef NOC_W18_METHODSSUB_378AA1_H
#define NOC_W18_METHODSSUB_378AA1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tui_gateway/methods_subagents.py (gate w18_methodssub_378aa1).
 * Pure roster helpers: child-session extraction and durable-owner resolution. */

/* PoP: w18_methodssub_378aa1_child_session_id @ tui_gateway/methods_subagents.py:_child_session_id
 * record.agent is a JSON object carrying a string "session_id"; returns JSON null when absent. */
json_t *w18_methodssub_378aa1_child_session_id(const json_t *record);
/* PoP: w18_methodssub_378aa1_stored_owner @ tui_gateway/methods_subagents.py:_stored_owner
 * db: {"sessions":[ids],"routing":{"<route>":"<sid>"}}; owner_ids: [stored_id, route];
 * namespaces: ["ns", ...]. Returns a JSON string ("" when not ours). */
json_t *w18_methodssub_378aa1_stored_owner(const json_t *db, const json_t *owner_ids,
                                           const json_t *namespaces);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_METHODSSUB_378AA1_H */
