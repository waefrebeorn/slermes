/*
 * noc_w29_oauth_dc3549_p2.h - C surface for upstream
 * hermes_cli/web_routers/oauth.py (gate w29_oauth_dc3549_p2).
 *
 * Device-code flow values: the Codex transport error classification, provider
 * error-detail extraction, the device-code start message, the usercode interval
 * clamp, the token-exchange form, the cancellation predicate, session GC, the
 * /start session+body shape and profile validation.  HTTP transports, the
 * session table and `secrets` are fixture seams.
 */
#ifndef NOC_W29_OAUTH_DC3549_P2_H
#define NOC_W29_OAUTH_DC3549_P2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP status -> the exception class name the Codex flow raises. */
json_t *w29_oauth_dc3549_p2_codex_http_error(const json_t *status);

/* req: {payload, text, json_raises} -> the short provider error detail string. */
json_t *w29_oauth_dc3549_p2_http_error_detail(const json_t *req);

/* req: {status, payload, text} -> the dashboard-facing device-code start error. */
json_t *w29_oauth_dc3549_p2_codex_start_error(const json_t *req);

/* device-code response -> {interval, ok, error} (interval clamped to >= 3s). */
json_t *w29_oauth_dc3549_p2_codex_user_code(const json_t *device_data);

/* req: {code_resp, client_id} -> {error, data} for the token exchange form. */
json_t *w29_oauth_dc3549_p2_codex_exchange(const json_t *req);

/* session dict -> whether the user cancelled it. */
json_t *w29_oauth_dc3549_p2_codex_cancelled(const json_t *sess);

/* req: {now, sessions:[{session_id, created_at}]} -> surviving session ids. */
json_t *w29_oauth_dc3549_p2_gc_survivors(const json_t *req);

/* req: {session_id, provider_id, profile, created_at, fields, user_code,
 *       verification_url, expires_in, poll_interval} -> {session, body}. */
json_t *w29_oauth_dc3549_p2_device_start(const json_t *req);

/* requested profile -> {name, resolved}. */
json_t *w29_oauth_dc3549_p2_validate_profile(const json_t *profile);

#ifdef __cplusplus
}
#endif
#endif
