/*
 * google_oauth.h — Google OAuth credential management for Hermes C.
 * Port of Python agent/google_oauth.py.
 *
 * Manages OAuth tokens for Google Gemini API access.
 * Uses ~/.hermes/auth/google_credentials.json for persistent storage.
 */

#ifndef HERMES_GOOGLE_OAUTH_H
#define HERMES_GOOGLE_OAUTH_H

#include <stdbool.h>
#include <stddef.h>

/* For oauth_token_t */
#include "hermes_auth.h"

/* For json_t */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Data structures ─────────────────────────────────────────── */

/* Google OAuth credentials */
typedef struct {
    char *access_token;
    char *refresh_token;
    char *id_token;
    char *scope;
    char *token_type;
    double expires_at;   /* Unix timestamp when access_token expires (0 = unknown) */
    char *email;
    char *project_id;
    char *managed_project_id;
} google_oauth_creds_t;

/* ─── Lifecycle ───────────────────────────────────────────────── */

/* Free all fields in creds struct */
void google_oauth_creds_free(google_oauth_creds_t *creds);

/* ─── Credential I/O ──────────────────────────────────────────── */

/* Load Google OAuth credentials from ~/.hermes/auth/.
 * Port of Python: load_credentials */
google_oauth_creds_t *google_oauth_load_credentials(void);

/* Save credentials to disk with 0o600 permissions (atomic write).
 * Port of Python: save_credentials */
bool save_credentials(const google_oauth_creds_t *creds);

/* Remove the credential file. Idempotent.
 * Port of Python: clear_credentials */
void clear_credentials(void);

/* ─── Token resolution ────────────────────────────────────────── */

/* Get a valid access token. If the stored token is expired and a
 * refresh_token is available, automatically refresh it.
 * Port of Python: get_valid_access_token */
char *google_oauth_get_valid_token(char **out_error);

/* Refresh the stored Google OAuth token using refresh_token.
 * Port of Python: refresh_access_token */
bool google_oauth_refresh_token(void);

/* Fetch user email from Google People API using the current access token.
 * Port of Python: _fetch_user_email */
char *fetch_user_email(void);

/* Set the hermes home directory (for testing, or override HERMES_HOME). */
void google_oauth_set_home(const char *home);

/* Set Google OAuth client_id and client_secret. */
void google_oauth_set_client_credentials(const char *client_id, const char *client_secret);

/* Port of Python: _get_client_id */
const char *google_oauth_get_client_id(void);

/* Port of Python: _get_client_secret */
const char *google_oauth_get_client_secret(void);

/* Port of Python: _require_client_id (env GOOGLE_CLIENT_ID, else default). */
const char *require_client_id(void);

/* Port of Python: _get_client_secret (raw; env GOOGLE_CLIENT_SECRET
 * overrides the compiled-in default). */
const char *get_client_secret(void);

/* Port of Python: exchange_code.
 * Wraps oauth_exchange_code() from token_exchange.c.
 * Returns allocated token, or NULL on failure. */
oauth_token_t *google_oauth_exchange_code(const char *auth_code, const char *code_verifier,
                                           const char *redirect_uri);

/* Port of Python: update_project_ids */
bool update_project_ids(const char *project_id, const char *managed_project_id);

/* Port of Python: _is_headless. Check if running without a desktop display. */
bool is_headless(void);

/* Port of Python: resolve_project_id_from_env. Read project ID from env vars. */
const char *resolve_project_id_from_env(void);

/* Port of Python: _lock_path. Return malloc'd lock file path (caller free). */
char *lock_path(void);

/* Port of Python: _prompt_paste_fallback. Prompt user to paste auth code from stdin. */
char *prompt_paste_fallback(void);

/* Port of Python: _locate_gemini_cli_oauth_js. Find gemini oauth2.js file. Returns malloc'd or NULL. */
char *locate_gemini_cli_oauth_js(void);

/* Port of Python: _persist_token_response. Save token response to credentials file.
 * Returns allocated creds struct (caller must google_oauth_creds_free + free). */
google_oauth_creds_t *google_oauth_persist_token_response(
    const char *access_token,
    const char *refresh_token,
    int expires_in,
    const char *project_id);

/* Port of Python: _generate_pkce_pair.
 * Generate RFC 7636 PKCE (verifier, challenge) pair using S256.
 * Each output is malloc'd; caller must free both via free().
 * Returns true on success. */
bool generate_pkce_pair(char **out_verifier, char **out_challenge);

/* Port of Python: _post_form.
 * POST application/x-www-form-urlencoded to url and return parsed JSON response.
 * keys/values arrays are num_pairs long. Returns json_t* (caller json_free)
 * or NULL on error. */
json_t *google_oauth_post_form(const char *url,
                               const char **keys, const char **values, int num_pairs,
                               int timeout_sec);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GOOGLE_OAUTH_H */
