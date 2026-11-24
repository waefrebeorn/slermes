/*
 * noc_w26_hostedroom_f8aecd.h - C surface for upstream
 * gateway/hosted_rooms_gateway_import.py (gate w26_hostedroom_f8aecd).
 *
 * The standalone backend's Group Chats move into the gateway once: which store
 * is the source, what authority a fenced room is handed to, and which room
 * services get a post-ready import pass.  The stores, the files and the
 * threads are seams -- this file owns only what the values decide.
 */
#ifndef NOC_W26_HOSTEDROOM_F8AECD_H
#define NOC_W26_HOSTEDROOM_F8AECD_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* source_for(target_db): the sibling ``shared-state.db`` when the target is a
 * root ``state.db`` and the sibling exists, else null. */
json_t *w26_hostedroom_f8aecd_source_for(const char *target_name, int sibling_exists);

/* handoff_gateway_id(local_id) -> "<local_id>:gateway-store". */
json_t *w26_hostedroom_f8aecd_handoff_id(const char *local_id);

/* import_after_ready's plan: [{"index": i, "start": bool}] for the authorities
 * that get a ready-import thread (no service, or one already started, skips). */
json_t *w26_hostedroom_f8aecd_import_plan(const json_t *authorities);

/* The module's literal constants and log templates, as an object. */
json_t *w26_hostedroom_f8aecd_constants(void);

#ifdef __cplusplus
}
#endif
#endif
