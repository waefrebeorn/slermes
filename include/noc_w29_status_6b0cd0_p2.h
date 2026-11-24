/*
 * noc_w29_status_6b0cd0_p2.h - C surface for upstream
 * hermes_cli/web_routers/status.py, gate p2 (payload shaping, predicates and
 * query parsing).
 *
 * The same fixture seams as p1: auth middleware, session store, config loader,
 * portal/curator modules and the log tail reader are inputs here, never calls.
 * This gate owns the shapes and decisions those inputs feed --
 *   * the Webapp signed-out predicate and the signed-out key allowlist,
 *   * the active-session counter predicate,
 *   * the Nous feature-state enum and the portal payload,
 *   * the /api/logs query parse (level/component/lines/search) and errors,
 *   * the diagnostics body guards (debug share consent + line clamp,
 *     retirement action/token validation), and
 *   * the curator status defaults and the local reads payload.
 */
#ifndef NOC_W29_STATUS_6B0CD0_P2_H
#define NOC_W29_STATUS_6B0CD0_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w29_status_6b0cd0_signed_out @ hermes_cli/web_routers/status.py:_webapp_caller_signed_out
 * req: {verified, surface, auth_required} -> unverified caller behind the
 * Webapp's sign-in gate. */
json_t *w29_status_6b0cd0_signed_out(const json_t *req);

/* PoP: w29_status_6b0cd0_signed_out_filter @ hermes_cli/web_routers/status.py:get_status
 * The inline signed-out projection: only _WEBAPP_SIGNED_OUT_STATUS_KEYS
 * survive (version, release_date, gateway_running, gateway_state, overall,
 * components, auth_required, auth_providers, auth_flows). */
json_t *w29_status_6b0cd0_signed_out_filter(const json_t *status);

/* PoP: w29_status_6b0cd0_active_sessions @ hermes_cli/web_routers/status.py:_count_status_active_sessions
 * req: {sessions: [{ended_at, last_active}], now} -> sessions with no
 * ended_at whose last activity is under 300s old. */
json_t *w29_status_6b0cd0_active_sessions(const json_t *req);

/* PoP: w29_status_6b0cd0_feature_state @ hermes_cli/web_routers/status.py:_feature_state
 * feat: {managed_by_nous, active, current_provider} -> "via Nous Portal" /
 * the provider / "active" / "not configured". */
json_t *w29_status_6b0cd0_feature_state(const json_t *feat);

/* PoP: w29_status_6b0cd0_portal_status @ hermes_cli/web_routers/status.py:_get_portal_status_sync
 * req: {cfg, auth, features, features_unavailable} -> the /api/portal payload.
 * load_config/get_nous_auth_status_local/get_nous_subscription_features are
 * the seams. */
json_t *w29_status_6b0cd0_portal_status(const json_t *req);

/* PoP: w29_status_6b0cd0_log_query @ hermes_cli/web_routers/status.py:get_logs
 * req: {file, lines, level, component, search, log_files, component_prefixes,
 *       path_exists, lines_content} -> {"error"} | {"file","lines"} |
 * {"file","lines","tail_n","has_filters","min_level","component_prefixes"}.
 * The tail read itself is the seam; ``lines_content`` is its result. */
json_t *w29_status_6b0cd0_log_query(const json_t *req);

/* PoP: w29_status_6b0cd0_debug_share @ hermes_cli/web_routers/status.py:run_debug_share_endpoint
 * req: {consent, lines} -> {"log_lines": n} after the consent gate and the
 * max(1, min(int(lines), 5000)) clamp, or a 400. */
json_t *w29_status_6b0cd0_debug_share(const json_t *req);

/* PoP: w29_status_6b0cd0_retirement @ hermes_cli/web_routers/status.py:post_health_retirement
 * req: {mounted, json_invalid, body, prepare_result, action_result} ->
 * {"error","detail","result"}.  The retirement ledger is the seam. */
json_t *w29_status_6b0cd0_retirement(const json_t *req);

/* PoP: w29_status_6b0cd0_curator_status @ hermes_cli/web_routers/status.py:get_curator_status
 * req: {curator: {...}, raises: [...]} -> the seven-key curator status with
 * _safe_call defaults for missing/failing accessors. */
json_t *w29_status_6b0cd0_curator_status(const json_t *req);

/* PoP: w29_status_6b0cd0_local_reads @ hermes_cli/web_routers/status.py:_status_local_reads
 * req: {version, config_version: [current, latest], managed_externally, nous}
 * -> {version, config_version, latest_config_version, can_update_hermes,
 * nous_session_valid}. */
json_t *w29_status_6b0cd0_local_reads(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W29_STATUS_6B0CD0_P2_H */
