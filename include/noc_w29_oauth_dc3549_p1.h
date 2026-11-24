/*
 * noc_w29_oauth_dc3549_p1.h - C surface for upstream
 * hermes_cli/web_routers/oauth.py (gate w29_oauth_dc3549_p1).
 *
 * Accounts-tab surface values: token preview, epoch-ms -> ISO, the status card
 * shapes, the per-provider status dispatch, and the disconnect decisions
 * (command, hint, HTTP error, rejection detail).  The auth store itself is a
 * fixture seam: every status getter is called with a canned raw dict.
 */
#ifndef NOC_W29_OAUTH_DC3549_P1_H
#define NOC_W29_OAUTH_DC3549_P1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* value + visible -> "…XXXXXX" tail (or "" for a falsy token).  A fixture value
 * of {"__callable__": true} stands for a callable (Entra-ID bearer) token. */
json_t *w29_oauth_dc3549_p1_truncate_token(const json_t *value, double visible);

/* epoch milliseconds -> aware ISO-8601 string ("...+00:00"), null when falsy. */
json_t *w29_oauth_dc3549_p1_epoch_ms_to_iso(const json_t *value);

/* req: {raw, source, source_label, token_preview, expires_at,
 *       has_refresh_token, last_refresh_present, last_refresh} -> status card. */
json_t *w29_oauth_dc3549_p1_status_card(const json_t *req);

/* req: {provider_id, raw, status_fn, raises} -> the provider's status card
 * (hand-written per-id shape, or the slug-driven generic card). */
json_t *w29_oauth_dc3549_p1_resolve_provider_status(const json_t *req);

/* host platform -> the command that removes Claude Code's borrowed credential. */
json_t *w29_oauth_dc3549_p1_claude_disconnect_command(const char *platform);

/* provider dict + platform -> the external disconnect command, or null. */
json_t *w29_oauth_dc3549_p1_provider_disconnect_command(const json_t *provider, const char *platform);

/* provider dict + status dict -> the manual disconnect hint, or null. */
json_t *w29_oauth_dc3549_p1_provider_disconnect_hint(const json_t *provider, const json_t *status);

/* status code + provider name -> {status_code, detail}. */
json_t *w29_oauth_dc3549_p1_disconnect_http_error(const json_t *status_code, const char *provider_name);

/* provider dict + status dict -> the 400 detail text, or null when allowed. */
json_t *w29_oauth_dc3549_p1_reject_detail(const json_t *provider, const json_t *status);

/* requested profile -> the stored profile name (null for ""/"current"). */
json_t *w29_oauth_dc3549_p1_profile_name(const json_t *profile);

#ifdef __cplusplus
}
#endif
#endif
