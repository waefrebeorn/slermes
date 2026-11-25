/*
 * noc_w34_sessions_67685b.h - C surface for upstream
 * tui_gateway/contracts/sessions.py (gate w34_sessions_67685b).
 *
 * The session topic's wire contract as values: the method catalog (33 names and
 * the params/result model each rides), every declared field of every model the
 * module owns, the closed enums, and the two envelope decisions made before a
 * handler runs — which keys a params object may carry and which it must carry.
 *
 * Pure value -> value. No session, agent, transcript, replay buffer or socket
 * is touched; the runtime seams (pydantic's validators, the JSON-RPC transport,
 * the live session itself) stay in their owners.
 */
#ifndef NOC_W34_SESSIONS_67685B_H
#define NOC_W34_SESSIONS_67685B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Every method this module declares, sorted by name:
 * [{"method": str, "params": str, "result": str}]. */
json_t *w34_sessions_67685b_catalog(void);

/* Every model this module declares, names sorted. */
json_t *w34_sessions_67685b_models(void);

/* One model's shape, or null for an unknown name:
 * {"model", "extra": "allow"|"forbid", "populate_by_name": bool,
 *  "keys": [sorted accepted input spellings],
 *  "fields": [{"name", "kind", "required", "factory", "alias", "min_length",
 *              "default"?}]}   (default present only for a value default) */
json_t *w34_sessions_67685b_fields(const char *model);

/* A model's required field names, sorted; null for an unknown name. */
json_t *w34_sessions_67685b_required(const char *model);

/* Params envelope check against the method's params model:
 * {"found": bool, "unknown_keys": [...], "missing_keys": [...],
 *  "error": str|null, "shape_ok": bool}.
 * An unknown key yields the gateway's 4000 text with the key path. */
json_t *w34_sessions_67685b_validate(const char *method, const json_t *params);

/* How a method's params name the session:
 * {"model", "has_session_id", "session_id_required", "has_session_key",
 *  "requirement": "session_id"|"either"|"optional"|"none"}; null if unknown. */
json_t *w34_sessions_67685b_id_requirement(const char *method);

/* A closed enum's values, sorted; null for an unknown name. */
json_t *w34_sessions_67685b_enum(const char *name);

/* Is `key` an accepted input spelling of the method's params model?
 * null when the method is not this module's. */
json_t *w34_sessions_67685b_accepts(const char *method, const char *key);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W34_SESSIONS_67685B_H */
