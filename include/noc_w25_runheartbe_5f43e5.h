/*
 * noc_w25_runheartbe_5f43e5.h - C surface for upstream
 * gateway/run_heartbeat_acceptance.py (gate w25_runheartbe_5f43e5).
 *
 * The pure decision core of an exact heartbeat admission: the lineage check
 * that keeps a tick from a superseded session out of the agent runner, the
 * refund of an attempt that never executed, the identity an admission is
 * deduplicated on and the pending-admission scan that coalesces overdue ticks.
 * The seams -- the session store, the persistence handle, the executor, the
 * HeartbeatManager's clock/state and the adapter slot -- stay in the caller.
 */
#ifndef NOC_W25_RUNHEARTBE_5F43E5_H
#define NOC_W25_RUNHEARTBE_5F43E5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The message id an admission is keyed on: json.dumps of this prefix plus the
 * session id, the state's created_at and its fire_count. */
#define W25_RUNHEARTBE_5F43E5_IDENTITY_PREFIX "heartbeat"

/* {"session_id": <string|null>, "created_at": <any>, "fire_count": <any>} ->
 * the identity string ("[\"heartbeat\",<sid>,<created_at>,<fire_count>]"). */
json_t *w25_runheartbe_5f43e5_identity(const json_t *req);

/* {"expected": <any>, "current": {"session_id": ..., "suspended": ...}|null} ->
 * bool: the resolved session is still the one the store holds. */
json_t *w25_runheartbe_5f43e5_owner_is_current(const json_t *req);

/* {"expected": <any>, "resolved": <any>, "tip": <any>, "current": {...}|null}
 * -> bool: normal reset/topic resolution, then only the original lineage. */
json_t *w25_runheartbe_5f43e5_resolve_owner(const json_t *req);

/* {"execution_started": <any>, "abandon_raises": <any>} ->
 * {"abandoned": bool, "warned": bool}. */
json_t *w25_runheartbe_5f43e5_settle(const json_t *req);

/* {"pending": [{"payload": {...}} ...]} -> bool: an admission is already
 * queued for this route (native_text_v1 or local_automation_v1 heartbeat). */
json_t *w25_runheartbe_5f43e5_pending_has_heartbeat(const json_t *req);

/* The identity/payload literal tables, as an object. */
json_t *w25_runheartbe_5f43e5_constants(void);

#ifdef __cplusplus
}
#endif
#endif
