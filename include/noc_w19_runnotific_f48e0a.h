/* noc_w19_runnotific_f48e0a.h - public C API for gateway/run_notifications_ownership.py. */
#ifndef NOC_W19_RUNNOTIFIC_F48E0A_H
#define NOC_W19_RUNNOTIFIC_F48E0A_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * The session store is modeled as data so the ownership decisions are pure:
 *   sessions:   [{"session_id","ended_at","end_reason","source","model_config"}]
 *   tips:       {"<session_id>": "<continuation session id>"}  (store.aio.lineage.tip)
 *   get_raises: ["<session_id>"]                               (store.aio.sessions.get raises)
 *   tip_raises: ["<session_id>"]                               (store.aio.lineage.tip raises)
 * A row's model_config is the stored JSON object (_delegate_from lives there).
 */

/* PoP: w19_runnotific_f48e0a_lookup_owner @ gateway/run_notifications_ownership.py:GatewayNotificationOwnershipMixin._lookup_completion_owner
 * -> {"verdict","session","row"} */
json_t *w19_runnotific_f48e0a_lookup_owner(json_t *sessions, json_t *tips,
                                           json_t *get_raises, json_t *tip_raises,
                                           const char *session_id,
                                           const char *current_session_id,
                                           int store_none);

/* PoP: w19_runnotific_f48e0a_compression_target @ gateway/run_notifications_ownership.py:GatewayNotificationOwnershipMixin._resolve_compression_lineage_target
 * -> {"verdict","target"} */
json_t *w19_runnotific_f48e0a_compression_target(json_t *sessions, json_t *tips,
                                                 json_t *entry, const char *pinned_session_id,
                                                 int get_raises, int tip_raises);

/* PoP: w19_runnotific_f48e0a_closed_local_target @ gateway/run_notifications_ownership.py:GatewayNotificationOwnershipMixin._closed_local_target
 * -> the physical closed session id, or "" */
json_t *w19_runnotific_f48e0a_closed_local_target(json_t *sessions, json_t *tips,
                                                  json_t *entry, const char *pinned_session_id,
                                                  json_t *pinned_row, int tip_raises);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_RUNNOTIFIC_F48E0A_H */
