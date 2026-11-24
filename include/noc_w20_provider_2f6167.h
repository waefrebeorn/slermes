/*
 * noc_w20_provider_2f6167.h -- public C API for the port of upstream Hermes
 *   plugins/browser/browser_use/provider.py (gate w20_provider_2f6167).
 *
 * Ported: the dual-auth config resolution (_get_config_or_none + _get_config
 * with their exact ValueError texts), _headers, _release_body, the managed
 * create payload, _should_preserve_pending_create_key, the pending
 * idempotency-key map (_get_or_create_pending_create_key /
 * _clear_pending_create_key) and the create_session record shaping. The
 * transport (_post_create / _check_created) lives in the shared base class and
 * is the seam: `response` is what the HTTP layer returned.
 */
#ifndef NOC_W20_PROVIDER_2F6167_H
#define NOC_W20_PROVIDER_2F6167_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* True when a failed create must keep its idempotency key: any 5xx, or a 409
 * whose payload says "already in progress". */
int w20_provider_2f6167_should_preserve_pending_create_key(int status_code,
                                                           const json_t *payload);

/* Seam inputs, mirroring the process state the Python reads:
 *   api_key            -- get_secret("BROWSER_USE_API_KEY"), "" / NULL for None
 *   selected           -- read_selection("browser"); absent key for None
 *   managed_available  -- resolve_managed_tool_gateway(...) is not None
 *   managed_token      -- managed.nous_user_token
 *   managed_origin     -- managed.gateway_origin
 *   tools_enabled      -- managed_nous_tools_enabled()
 * Returns {"config": {...}|null, "error": "..."|null}. */
json_t *w20_provider_2f6167_resolve_config(const json_t *seams);

/* {"Content-Type": "application/json", "X-Browser-Use-API-Key": api_key} */
json_t *w20_provider_2f6167_headers(const json_t *config);

/* {"action": "stop"} */
json_t *w20_provider_2f6167_release_body(void);

/* Managed sessions pin a short timeout and a proxy country; direct ones send
 * no body at all. */
json_t *w20_provider_2f6167_create_payload(int managed_mode);

/* Replays a sequence of map operations so reuse/clear semantics are observable:
 *   ops: [{"op":"get","task_id":"t1"}, {"op":"clear","task_id":"t1"}, ...]
 * Returns one record per op: get -> {"kind":"key","reused":bool,
 * "prefix":"browser-use-session-create:","hex_len":32}; clear -> {"kind":"void"}.
 * The module-level map persists across calls exactly like the Python global. */
json_t *w20_provider_2f6167_pending_key_ops(const json_t *ops);

/* The whole create_session flow against a scripted transport response
 * (`response` = {"ok":bool,"status":int,"body":{...},"headers":{...}}).
 * Returns {"status":"ok","value":{...}} or
 * {"status":"raised","type":"ValueError"|"KeyError","message":"..."}.
 * The record reports "session_name_prefix" (the adapter mints a random
 * 8-hex suffix, so only the deterministic prefix is comparable). */
json_t *w20_provider_2f6167_create_session(const json_t *seams, const char *task_id,
                                           const json_t *response);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_PROVIDER_2F6167_H */
