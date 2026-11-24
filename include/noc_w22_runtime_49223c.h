#ifndef NOC_W22_RUNTIME_49223C_H
#define NOC_W22_RUNTIME_49223C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/teams_pipeline/runtime.py (gate w22_runtime_49223c).
 *
 * The module wires the Teams meeting pipeline into the gateway: it mirrors the
 * Teams platform settings into the pipeline's ``teams_delivery`` block, decides
 * whether delivery is actually configured, and attaches a notification
 * scheduler to the msgraph webhook adapter (with a drop-scheduler fallback).
 * The pipeline object itself, the graph client and the store are host seams of
 * their own modules; what lives here is the config shaping and the attach
 * ladder.
 */

/* PoP: w22_runtime_49223c_delivery_configured @ plugins/teams_pipeline/runtime.py:_teams_delivery_is_configured
 * `delivery_mode = str(teams_delivery.get("mode") or pick("delivery_mode") or "").strip().lower()`
 * where `pick(key) = teams_delivery.get(key) or teams_extra.get(key)`.
 *   "incoming_webhook" -> bool(pick("incoming_webhook_url"))
 *   "graph"            -> bool(pick("chat_id") or (pick("team_id") and pick("channel_id")))
 *   anything else      -> false
 * Answer: a JSON boolean. */
json_t *w22_runtime_49223c_delivery_configured(const json_t *teams_extra,
                                              const json_t *teams_delivery);

/* PoP: w22_runtime_49223c_config @ plugins/teams_pipeline/runtime.py:build_pipeline_runtime_config
 * Input is the gateway's ``platforms`` mapping as JSON:
 *   {"teams": {"enabled": <bool>, "extra": {...}}}   (absent/null -> no entry)
 * Answers the pipeline config dict: a copy of ``extra.meeting_pipeline`` (or {})
 * with the Teams delivery mirror applied when the entry exists and is enabled.
 * Delivery keys copied only when the value is neither null nor "". */
json_t *w22_runtime_49223c_config(const json_t *platforms);

/* PoP: w22_runtime_49223c_bind @ plugins/teams_pipeline/runtime.py:bind_gateway_runtime
 * The attach ladder against a scripted gateway:
 *   req = {"adapter": <bool>, "runtime": <bool>, "build_error": <str>|null}
 *      adapter      -- the msgraph webhook adapter is registered
 *      runtime      -- gateway._teams_pipeline_runtime is already set
 *      build_error  -- build_pipeline_runtime raises with this message
 * Answer: {"result": bool, "scheduler": "_drop"|"_schedule"|null,
 *          "error": <str>|null, "runtime_installed": bool}.
 * `error` is the value gateway._teams_pipeline_runtime_error holds after the
 * call (None in every path but the build failure). */
json_t *w22_runtime_49223c_bind(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_RUNTIME_49223C_H */
