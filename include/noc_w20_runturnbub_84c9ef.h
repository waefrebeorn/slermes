/*
 * noc_w20_runturnbub_84c9ef.h -- public C API for the port of upstream Hermes
 *   gateway/run_turn_bubble_cleanup.py (gate w20_runturnbub_84c9ef).
 *
 * Ported: DELETE_TIMEOUT_SECONDS, the register_bubble_cleanup admission test
 * plus the ids/chat_id snapshot it hands the callback, and the failure tally
 * behind the "Temp bubble cleanup for session ..." line. The coroutines
 * themselves (asyncio.wait_for around adapter.delete_message,
 * register_post_delivery_callback, asyncio.wait over the turn's senders) need a
 * live event loop and a live adapter, so they stay upstream.
 */
#ifndef NOC_W20_RUNTURNBUB_84C9EF_H
#define NOC_W20_RUNTURNBUB_84C9EF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 10.0 -- the per-delete bound and the handoff's sender wait. */
double w20_runturnbub_84c9ef_delete_timeout_seconds(void);

/* The admission test of register_bubble_cleanup and the snapshot it captures.
 * `ctx` mirrors turn_ctx: {"cleanup_progress":bool,"has_adapter":bool,
 * "ids":[...],"session_key":"...","generation":"...","chat_id":"...",
 * "has_register_cb":bool}; `response` is the delivery result (any JSON).
 * Returns {"register":bool,"ids":[...],"chat_id":...}; ids/chat_id are only
 * filled when the callback is registered. */
json_t *w20_runturnbub_84c9ef_register(const json_t *ctx, const json_t *response);

/* The tally _cleanup_temp_bubbles logs. `spec` is
 * {"session_key":"...","results":["ok"|"false"|"raise", ...]} where an entry is
 * what `await _delete(mid)` did: returned a truthy value, returned False, or
 * raised (both of the latter count as failed).
 * Returns {"failed":int,"message":"..."|null}. */
json_t *w20_runturnbub_84c9ef_cleanup(const json_t *spec);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_RUNTURNBUB_84C9EF_H */
